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

#include "value.hpp"
#include "eval.hpp"

#include "tool.hpp"
#include "vm.hpp"


template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);

	std::stringstream ss(line);
	f(ss);
    
  }
  
};




static const auto evaluate = [] {
  ref<lisp::context> ctx = lisp::std_env();
  
  return [ctx](lisp::value&& e) {
    const lisp::value val = eval(ctx, e);
    if(val) {
      std::cout << val << std::endl;  
    }
    return parse::pure(e);
  };
};




template<class Action>
static int process(std::istream& in, Action action) {
  
  const auto parse = *( lisp::parser() >> action | parse::error<lisp::value>() );
  
  try{
    parse(in);
    return 0;
  } catch(parse::error<lisp::value>& e) {
    std::cerr << e.what() << std::endl;
  }
  catch( lisp::error& e ) {
    std::cerr << "error: " << e.what() << std::endl;
  }

  return 1;
}



const auto jit = [] {

  ref<vm::jit> jit = make_ref<vm::jit>();

  jit->import( lisp::std_env() );
  
  return [jit](lisp::value&& e) {

    const lisp::value val = jit->eval(e);

    if(val) {
      std::cout << val << std::endl;  
    }
    return parse::pure(e);
  };
  
  

};


int main(int argc, char** argv) {
  using namespace lisp;

  // const auto action = interpret();
  const auto action = jit();  
  
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
 
 
