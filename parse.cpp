// -*- compile-command: "c++ -std=c++11 parse.cpp -o parse" -*-

#include "parse.hpp"

#include <iostream>
#include <sstream>



int main(int, char** ) {

  using namespace parse;
    
  auto alpha = chr<std::isalpha>();
  auto alnum = chr<std::isalnum>();
  auto space = chr<std::isspace>();
    
  auto endl = chr('\n');
  auto comment = (chr(';'), *!endl, endl);

  auto real = lit<double>();
  auto integer = lit<int>();

  auto dquote = chr('"');

  // TODO escaped strings
  auto string = no_skip( (dquote,
                          *!dquote,
                          dquote));
  
  auto identifier = no_skip( (alpha, *alnum) );

  auto atom = identifier | string | real | integer;
  
  struct expr_tag;
  any<expr_tag> expr;
  
  auto list = no_skip( (chr('('),
                        *space,
                        ~( expr, *(+space, expr) ),
                        *space,
                        chr(')' )) );
     
  expr = atom | list;
                      
  auto parser = expr;
   
  while(std::cin) {
	std::string line;
    
    std::cout << "> ";
	std::getline(std::cin, line);
	 
	std::stringstream ss(line);
	if(ss >> parser) {
	  std::cout << "parse succeded" << std::endl;
	} else if(!std::cin.eof()) {
	  std::cerr << "parse error" << std::endl;
	}
  }
  
}
 
 
