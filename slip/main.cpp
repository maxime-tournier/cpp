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


class prime_enumerator {
  using value_type = std::size_t;
  value_type current = 1;
  std::vector<value_type> previous;
public:
  
  value_type operator()() {
    
    while(true) {
      ++current;
      const value_type sqrt = std::sqrt(current);
      
      bool has_div = false;
      for(value_type p : previous) {
        if(current % p == 0) {
          has_div = true;
          break;
        }
        
        if( p > sqrt ) break;
      }
      
      if(!has_div) {
        previous.push_back(current);
        return current;
      }
    }
  }
};


template<class U>
static U gcd(U a, U b) {
  U res;
  
  while(U r = a % b) {
    a = b;
    b = r;
    res = r;
  }

  return res;
}


template<class U>
static void bezout(U a, U b, int* s, int* t) {
  
  int s_prev = 1, t_prev = 0;
  int s_curr = 0, t_curr = 1;
  
  while(U r = a % b) {
    const U q = a / b;

    const int s_next = s_prev - q * s_curr;
    const int t_next = t_prev - q * t_curr;    

    s_prev = s_curr;
    s_curr = s_next;
    
    t_prev = t_curr;
    t_curr = t_next;    
    
    a = b;
    b = r;
  }

  *s = s_curr;
  *t = t_curr;
  
}



template<class U>
static int chinese_euclid(U a, U b) {
  int e_prev = 0, e = b;
  
  while(U r = a % b) {
    const U q = a / b;
    
    const int e_next = e_prev - q * e;
    
    e_prev = e;
    e = e_next;
    
    a = b;
    b = r;
  }

  return e;
}


int mul_inv(int a, int b)
{
	int b0 = b, t, q;
	int x0 = 0, x1 = 1;
	if (b == 1) return 1;
	while (a > 1) {
		q = a / b;
		t = b, b = a % b, a = t;
		t = x0, x0 = x1 - q * x0, x1 = t;
	}
	if (x1 < 0) x1 += b0;
	return x1;
}

template<class U>
static int chinese_remainders(const U* a, const U* n, std::size_t size) {

  U prod = 1;
  for(std::size_t i = 0; i < size; ++i) {
    prod *= n[i];
  }
  
  int x = 0;

  for(std::size_t i = 0; i < size; ++i) {
    const U m = prod / n[i];

    int u, v;
    bezout(m, n[i], &u, &v);

    // if( u < 0 ) u += m;
    std::clog << "bezout: " << u << ", " << v << std::endl;
    
    const int e = m * u;
    std::clog << "e: " << e << std::endl;
    
    x += a[i] * e;
    // x += a[i] * mul_inv(m, n[i]) * m;    
    std::clog << "chinese: " << x << std::endl;
  }

  return x;
}



struct test {
  test() {

    prime_enumerator f;

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
 
 
