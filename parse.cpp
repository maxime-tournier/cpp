// -*- compile-command: "c++ -std=c++11 parse.cpp -o parse" -*-

#include "parse.hpp"

#include <iostream>
#include <sstream>

int main(int, char** ) {

  using namespace parse;
    
  auto alpha = chr([](char c) { return std::isalpha(c); });
  auto alnum = chr([](char c) { return std::isalnum(c); });
  auto space = chr([](char c) { return std::isspace(c); });
    
  auto endl = chr('\n');
  auto comment = (chr(';'), *!endl, endl);

  auto real = lit<double>();
  auto integer = lit<int>();
    
  auto identifier = no_skip( (alpha, *alnum) );

  auto atom = identifier | real | integer;
  
  struct expr_tag;
  rec<expr_tag> expr;
  
  auto list = no_skip( (chr('('),
                        *space,
                        ~( expr, *(+space, expr) ),
                        *space,
                        chr(')' )) );
    
  expr = atom | list;

  auto parser = list;

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
  
  // std::clog << "peek: " << char(ss.peek()) << std::endl;

  // // literal<std::string> rest;
  // // ss >> no_skip( rest );
  // std::string rest;
  // std::getline(ss, rest);
  // std::clog << "rest: " << rest << std::endl;
}
