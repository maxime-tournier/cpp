// -*- compile-command: "c++ -std=c++14 parser.cpp -o parser -lstdc++ -lm -g" -*-
#include "parser.hpp"
#include "variant.hpp"

#include <sstream>
#include <iostream>

#include "sexpr.hpp"

int main(int argc, char** argv) {
  if(argc <= 1) {
    std::cerr << "usage: " << argv[0] << " STRING" << std::endl;
    return 1;
  }
  
  using namespace parser;
  const auto parser = sexpr::parser() >>= drop(parser::eos);

  std::stringstream ss(argv[1]);

  try {
    auto result = run(parser, ss);
    std::cout << "success: " << result << std::endl;
    return 0;
  } catch(std::runtime_error& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }

  
}


