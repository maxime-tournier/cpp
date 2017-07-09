#include <iostream>
#include "parse.hpp"

#include <sstream>
#include <string>

#include <set>
#include <memory>
#include <vector>
#include <deque>
#include <cassert>
#include <fstream>

#include <unordered_map>
#include <map>

#include <readline/readline.h>
#include <readline/history.h>

#include "eval.hpp"

#include "tool.hpp"
#include "vm.hpp"
#include "syntax.hpp"

#include "jit.hpp"

#include "types.hpp"
#include "ast.hpp"

#include <numeric>
#include "../prime.hpp"

#include <boost/program_options.hpp>
namespace po = boost::program_options;


template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);
    
	std::stringstream ss(line);
	f(ss);
  }
  
};




struct test {
  test() {

    prime_enumerator<std::size_t> f;

    for(std::size_t i = 0, n = 20; i < n; ++i) {
      std::clog << f() << std::endl;
    }

    const std::size_t ns[] = {f(), f(), f(), f()};
    const std::size_t as[] = {0, 1, 2, 3};

    const int x = chinese_remainders(as, ns, 4);
    
    for(std::size_t i = 0; i < 4; ++i) {
      std::clog << x % (int)ns[i] << " vs. " << as[i] << std::endl;
    }

    std::clog << "gcd: " << gcd( ns[0], ns[1] ) << std::endl;
    std::clog << "gcd: " << gcd( ns[1], ns[2] ) << std::endl;
    std::clog << "gcd: " << gcd( ns[1], ns[3] ) << std::endl;

    int s, t;

    bezout(ns[0], ns[1], &s, &t);

    std::clog << ns[0] * s + ns[1] * t << std::endl;

    
  }

} instance;





template<class Action>
static int process(std::istream& in, Action action) {
  
  const auto parse = parse::start( slip::skipper() ) >>=
    *( slip::parser() >> action | parse::error<slip::sexpr>() );
  
  try{
    parse(in);
    return 0;
  }
  catch(parse::error<slip::sexpr>& e) {
    std::cerr << "parse error: " << e.what() << std::endl;
  }
  catch( slip::syntax_error& e ) {
    std::cerr << "syntax error: " << e.what() << std::endl;
  }

  catch( slip::type_error& e ) {
    std::cerr << "type error: " << e.what() << std::endl;
  }
  
  catch( slip::error& e ) {
    std::cerr << "runtime error: " << e.what() << std::endl;
  }  

  return 1;
}


static const auto interpreter = [] {
  const ref<slip::environment> ctx = slip::std_env();
  
  return [ctx](slip::sexpr&& e) {
    const slip::sexpr ex = expand_seq(ctx, e);
    const slip::value val = eval(ctx, ex);
    
    if(!val.is<slip::unit>()) {
      std::cout << val << std::endl;  
    }
    
    return parse::pure(e);
  };
};




namespace slip {

  struct helper {
    
    ref<slip::jit> jit;
    ref<types::context> tc;

    template<class T>
    helper& operator()(symbol name, types::mono type, const T& value) {
      tc->def(name, type);
      jit->def(name, value);
      return *this;   
    }

  
    template<class F, class Ret, class ... Args, std::size_t ... I>
    helper& operator()(symbol name, F f, Ret (*)(Args...args), indices<I...> ) {

      const types::mono args[] = { types::traits<Args>::type()... };
      const list<types::mono> arg_types = make_list<types::mono>(args, args + sizeof...(Args) );
    
      const types::mono result_type = types::traits<Ret>::type();
    
      types::mono app = foldr(result_type, arg_types, [](types::mono lhs, types::mono rhs) -> types::mono {
          return lhs >>= rhs;
        });

      // remember true argc
      app.get<types::application>().info = sizeof...(Args);
    
      static Ret (*ptr)(Args...) = +f;
    
      vm::builtin wrapper = [](const vm::value* first, const vm::value* last) -> vm::value {
        return ptr(first[I].get<Args>()...);
      };


      return (*this)(name, app, wrapper);
    }
  

    template<class Ret, class ... Args>
    static range_indices<sizeof...(Args)> args_indices(Ret (*ptr)(Args...) ) {
      return {};
    }

  
  
    template<class F>
    helper& operator()(symbol name, F f) {
      return (*this)(name, f, +f, args_indices(+f));
    }
  
  };
}

const auto jit_compiler = [](bool dump) {

  static const ref<slip::environment> env = make_ref<slip::environment>();
  static const ref<slip::jit> jit = make_ref<slip::jit>();
  static const ref<slip::types::context> tc = make_ref<slip::types::context>();

  slip::helper ctx{jit, tc};
  
  {
    using namespace slip;
    using namespace slip::types;

    ctx("+", [](integer lhs, integer rhs) -> integer { return lhs + rhs; });
    ctx("-", [](integer lhs, integer rhs) -> integer { return lhs - rhs; });
    ctx("/", [](integer lhs, integer rhs) -> integer { return lhs / rhs; });
    ctx("*", [](integer lhs, integer rhs) -> integer { return lhs * rhs; });
    ctx("%", [](integer lhs, integer rhs) -> integer { return lhs % rhs; });
    ctx("=", [](integer lhs, integer rhs) -> boolean { return lhs == rhs; });

    {
      ref<variable> a = tc->fresh();
      ref<variable> thread = tc->fresh();      
      tc->def(kw::ref, a >>= io_ctor(ref_ctor(a, thread), thread));
    }

    {
      ref<variable> a = tc->fresh();
      ref<variable> thread = tc->fresh();            
      tc->def(kw::set, ref_ctor(a, thread) >>= a >>= io_ctor(unit_type, thread));
    }

    {
      ref<variable> a = tc->fresh();
      ref<variable> thread = tc->fresh();
      tc->def(kw::get, ref_ctor(a, thread) >>= io_ctor(a, thread));
    }

    {
      ref<variable> a = tc->fresh();
      ref<variable> thread = tc->fresh();      
      tc->def(kw::pure, a >>= io_ctor(a, thread));
    }
    
    {
      ref<variable> a = tc->fresh();
      ctx("nil", list_ctor(a), vm::value::list());
    }

    {
      ref<variable> a = tc->fresh();
      tc->def("cons", a >>= list_ctor(a) >>= list_ctor(a) );
    }


    {
      // TODO typeclasses
      ref<variable> a = tc->fresh();
      ref<variable> thread = tc->fresh();      

      tc->def("print", a >>= io_ctor( unit_type, thread));
    }
    
    
    
  }


  // allow jit to create arrays
  jit->def("array", +[](const slip::vm::value* first, const slip::vm::value* last) -> slip::vm::value {
      return slip::vm::make_array(first, last);
    });
  
  
  // (*env)("iter", +[](const slip::value* first, const slip::value* last) -> slip::value {
  //     using namespace slip::vm;

  //     const auto args = slip::unpack_args<2>( (value*)first, (value*)last);
      
  //     const value::list& x = args[0].get<value::list>();
  //     const value& func = args[1];
      
  //     for(const value& xi : x) {
  //       jit->call(func, &xi, &xi + 1);
  //     }
      
  //     return unit();
  //   });
  

  // (*env)("map", +[](const slip::value* first, const slip::value* last) -> slip::value {
  //     using namespace slip::vm;

  //     const auto args = slip::unpack_args<2>( (value*)first, (value*)last);

  //     const value::list& x = args[0].get<value::list>();
  //     const value& func = args[1];

      
  //     value::list result = map(x, [&](const value& xi) {
  //         return jit->call(func, &xi, &xi + 1);
  //       });

  //     return reinterpret_cast<slip::value::list&>(result);
  //   });


  using namespace slip;
  
  return [dump](sexpr&& s) {

    const ast::toplevel node = ast::check_toplevel(s);
    std::cout << "ast: " << node << std::endl;
    
    const sexpr e = expand_toplevel(env, s);

    const types::check_type checked = types::check(tc, e);

    if(dump) {
      std::cout << "rewritten expr: " << checked.expr << std::endl;
    }
    
    const vm::value v = jit->eval(checked.expr, dump);
    
    std::cout << " : " << checked.type; 
    
    if(v.is<slip::unit>()) {
      std::cout << std::endl;
    } else {
      std::cout << " = ";
      
      if(v.is<slip::symbol>() || v.is<slip::vm::value::list>()) {
        std::cout << "'";
      }
      std::cout << v << std::endl;  
    }
    return parse::pure(e);
  };
  
  

};

namespace po = boost::program_options;
static po::variables_map parse_options(int argc, char** argv) {
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("filename", po::value< std::string >(), "input file")

    ("interpreter,i", "interpreter")
    ("bytecode,b", "bytecode")
    ("dump", "dump bytecode")    
    ;

  po::positional_options_description p;
  p.add("filename", -1);
  
  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
            options(desc).positional(p).run(), vm);
  
  po::notify(vm);
  
  if (vm.count("help")) {
    std::cout << desc << "\n";
    std::exit(1);
  }
  
  return vm;
}


int main(int argc, char** argv) {

  const po::variables_map vm = parse_options(argc, argv);

  using action_type = std::function< parse::any<slip::sexpr> (slip::sexpr&&) > ;
  
  action_type action = nullptr;

  
  if( vm.count("interpreter") ) {
    action = interpreter();
  } else {
    const bool dump = vm.count("dump");    
    action = jit_compiler(dump);
  }
  

  if( vm.count("filename") ) {
    const std::string filename = vm["filename"].as< std::string >();
    std::ifstream file( filename );
    
    return process(file, action );
    
  } else {
  
    read_loop([&](std::istream& in) {
        process(in, action );
      });
    
  }
 
  return 0;
}
 
 
