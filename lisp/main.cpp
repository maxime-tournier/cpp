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


template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);
    
	std::stringstream ss(line);
	f(ss);
  }
  
};




static const auto evaluate = [] {
  const ref<lisp::context> ctx = lisp::std_env();
  
  return [ctx](lisp::sexpr&& e) {
    const lisp::sexpr ex = expand_seq(ctx, e);
    const lisp::value val = eval(ctx, ex);
    
    if(val) {
      std::cout << val << std::endl;  
    }
    
    return parse::pure(e);
  };
};




template<class Action>
static int process(std::istream& in, Action action) {
  
  const auto parse = *( lisp::parser() >> action | parse::error<lisp::sexpr>() );
  
  try{
    parse(in);
    return 0;
  } catch(parse::error<lisp::sexpr>& e) {
    std::cerr << "parse error: " << e.what() << std::endl;
  }
  
  catch( lisp::syntax_error& e ) {
    std::cerr << "syntax error: " << e.what() << std::endl;
  }
  
  catch( lisp::error& e ) {
    std::cerr << "runtime error: " << e.what() << std::endl;
  }  

  return 1;
}



const auto jit_compile = [] {

  const ref<lisp::jit> jit = make_ref<lisp::jit>();
  const ref<lisp::context> env = lisp::std_env();
  
  jit->import( env );
  
  return [env, jit](lisp::sexpr&& e) {

    const lisp::sexpr ex = expand_seq(env, e);    
    const lisp::vm::value val = jit->eval(ex);

    if(val) {
      if(val.is<lisp::symbol>() || val.is<lisp::vm::value::list>()) {
        std::cout << "'";
      }
      std::cout << val << std::endl;  
    }
    return parse::pure(e);
  };
  
  

};


int main(int argc, char** argv) {
  
  // const auto action = interpret();
  const auto action = jit_compile();  
  
  if( argc > 1 ) {
    std::ifstream file(argv[1]);
    
    return process(file, action );
    
  } else {
  
    read_loop([&](std::istream& in) {
        process(in, action );
      });
    
  }
 
  return 0;
}
 
 
