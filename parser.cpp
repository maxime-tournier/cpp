// -*- compile-command: "c++ -std=c++14 parser.cpp -o parser -lstdc++ -g" -*-
#include "parser.hpp"

#include <cctype>
#include <string>
#include <iostream>

int main(int, char**) {
  using namespace parser;
  // maybe<char> m = 'c';

  std::string input = "foo";
  range in = {input.data(), input.data() + input.size()};
  
  const auto parser = chr<std::isalnum>;

  std::cout << parser::run(parser, in) << std::endl;
  
  
  return 0;
}
