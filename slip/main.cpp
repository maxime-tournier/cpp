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

#include "kinds.hpp"


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



static const auto jit_compiler = [](bool dump_bytecode) {
  using namespace slip;
  
  auto tc = make_ref<kinds::typechecker>();
  
  {
    kinds::monotype a = tc->fresh();
    tc->def("pure", tc->generalize( a >>= kinds::io_ctor(a) ));
  }

  {
    kinds::monotype a = tc->fresh();
    tc->def("nil", tc->generalize(kinds::list_ctor(a)));
  }
  
  
  return [tc](sexpr&& s) mutable {

    const ast::toplevel node = ast::check_toplevel(s);

    const kinds::polytype p = infer(*tc, node);
    std::cout << " : " << p << std::endl;

    return parse::pure(s);
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
 
 
