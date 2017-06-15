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
  
  const auto parse = *( lisp::parser() >> action | parse::error<lisp::sexpr>() );
  
  try{
    parse(in);
    return 0;
  }
  catch(parse::error<lisp::sexpr>& e) {
    std::cerr << "parse error: " << e.what() << std::endl;
  }
  catch( lisp::syntax_error& e ) {
    std::cerr << "syntax error: " << e.what() << std::endl;
  }

  catch( lisp::type_error& e ) {
    std::cerr << "type error: " << e.what() << std::endl;
  }
  
  catch( lisp::error& e ) {
    std::cerr << "runtime error: " << e.what() << std::endl;
  }  

  return 1;
}


static const auto interpreter = [] {
  const ref<lisp::environment> ctx = lisp::std_env();
  
  return [ctx](lisp::sexpr&& e) {
    const lisp::sexpr ex = expand_seq(ctx, e);
    const lisp::value val = eval(ctx, ex);
    
    if(!val.is<lisp::unit>()) {
      std::cout << val << std::endl;  
    }
    
    return parse::pure(e);
  };
};




const auto jit_compiler = [](bool dump) {

  static const ref<lisp::jit> jit = make_ref<lisp::jit>();
  static const ref<lisp::environment> env = lisp::std_env();

  static const ref<lisp::types::context> tc = make_ref<lisp::types::context>();

  {
    using namespace lisp::types;
    tc->def("+", integer_type >>= integer_type >>= integer_type);
    tc->def("-", integer_type >>= integer_type >>= integer_type);
    tc->def("/", integer_type >>= integer_type >>= integer_type);
    tc->def("*", integer_type >>= integer_type >>= integer_type);
    tc->def("%", integer_type >>= integer_type >>= integer_type);
    tc->def("=", integer_type >>= integer_type >>= boolean_type);                    
  }
           
  
  (*env)("iter", +[](const lisp::value* first, const lisp::value* last) -> lisp::value {
      using namespace lisp::vm;

      const auto args = lisp::unpack_args<2>( (value*)first, (value*)last);
      
      const value::list& x = args[0].get<value::list>();
      const value& func = args[1];
      
      for(const value& xi : x) {
        jit->call(func, &xi, &xi + 1);
      }
      
      return unit();
    });
  

  (*env)("map", +[](const lisp::value* first, const lisp::value* last) -> lisp::value {
      using namespace lisp::vm;

      const auto args = lisp::unpack_args<2>( (value*)first, (value*)last);

      const value::list& x = args[0].get<value::list>();
      const value& func = args[1];

      
      value::list result = map(x, [&](const value& xi) {
          return jit->call(func, &xi, &xi + 1);
        });

      return reinterpret_cast<lisp::value::list&>(result);
    });

  jit->import( env );

  using namespace lisp;  
  return [dump](sexpr&& s) {
    
    const sexpr e = expand_seq(env, s);

    const types::scheme t = types::check(tc, e);
    std::cout << " : " << t;

    if( t.body == types::unit_type ) std::cout << std::endl;
    else std::cout << " = ";

    const vm::value v = jit->eval(e, dump);
    
    if(!v.is<lisp::unit>()) {
      if(v.is<lisp::symbol>() || v.is<lisp::vm::value::list>()) {
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

  using action_type = std::function< parse::any<lisp::sexpr> (lisp::sexpr&&) > ;
  
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
 
 
