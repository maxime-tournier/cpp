// -*- compile-command: "c++ -std=c++14 parser.cpp -o parser -lstdc++ -g" -*-
#include "parser.hpp"

#include <cctype>
#include <string>
#include <iostream>

int main(int, char**) {
  using namespace parser;
  // maybe<char> m = 'c';

  std::stringstream ss;
  ss << " foo";
  
  const auto parser = chr<std::isalnum>;

  std::cout << parser::run(parser, ss) << std::endl;
  
  
  return 0;
}
