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

namespace lisp {
  

  
}

template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);

	std::stringstream ss(line);

	if( f(ss) ) {
      // success
	} else if(!std::cin.eof()) {
	  std::cerr << "parse error" << std::endl;
	}
    
  }
  
};






int main(int argc, char** argv) {
  using namespace lisp;
  ref<context> ctx = std_env(argc, argv);
  
  // clang chokes with default capture by reference
  const auto parser = *lisp::parser() >> [&ctx](std::deque<value>&& exprs) {
    try{

      for(const value& e : exprs) {
        const value val = eval(ctx, e);
        if(val) {
          std::cout << val << std::endl; 
        }
      }
      
    } catch( error& e ) {
      std::cerr << "error: " << e.what() << std::endl;
    }
    
    return parse::pure(exprs);
  };
  

  if( argc > 1 ) {
    std::ifstream file(argv[1]);
    
    if(!parser(file)) {
      std::cerr << "parse error" << std::endl;
      return 1;
    }
    
  } else {
  
    read_loop([parser](std::istream& in) {
        return bool( parser(in) );
      });
  }
 
  return 0;
}
 
 
