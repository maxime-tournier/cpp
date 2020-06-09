// -*- compile-command: "c++ -std=c++14 parser.cpp -o parser -lstdc++ -g" -*-
#include "parser.hpp"

#include "variant.hpp"

#include <cctype>
#include <string>
#include <iostream>


struct sexpr: variant<long, double, std::string, std::deque<sexpr>> {
  using sexpr::variant::variant;
  using list = std::vector<sexpr>;
};

int main(int argc, char** argv) {
  using namespace parser;
  // maybe<char> m = 'c';

  const auto lparen = token(single<'('>);
  const auto rparen = token(single<')'>);  

  const auto cast = [](auto value) { return sexpr(value); };
  
  const auto integer = map(cast, _long);
  const auto number = map(cast, _double);
  const auto space = _char<std::isspace>;
  
  const any<sexpr> expr = integer | number | (lparen >> map(cast, (ref(expr) % space)) >>= drop(rparen));
  
  // const auto parser = chr<std::isalnum>;
  const auto parser = ref(expr);

  std::stringstream ss;
  ss << "";
  
  auto result = parser::run(parser, ss);
  // std::clog << result.size() << std::endl;
  
  return 0;
}
