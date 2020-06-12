// -*- compile-command: "c++ -std=c++14 parser.cpp -o parser -lstdc++ -lm -g" -*-
#include "parser.hpp"

#include "variant.hpp"

#include <cctype>
#include <cmath>

#include <string>
#include <iostream>


struct sexpr: variant<long, double, std::string, std::deque<sexpr>> {
  using sexpr::variant::variant;
  using list = std::deque<sexpr>;

  friend std::ostream& operator<<(std::ostream& out, const sexpr& self) {
    match(self,
          [&](const auto& value) { out << value << typeid(value).name(); },
          [&](const std::string& value) {
            out << '\"' << value << '\"';
          },
          [&](const list& values) {
            out << '(';
            bool first = true;
            for(const auto& value: values) {
              if(first) first = false;
              else out << ' ';
              out << value;
            }
            out << ')';
          });
    return out;
  }

  static auto parser() {
    using namespace parser;

    const auto lparen = debug("lparen", token(single('(')));
    const auto rparen = debug("rparen", token(single(')')));

    const auto quote = single('"'); 
    const auto not_quote = pred([](char x) { return x != '"'; });
    
    const auto backslash = single('\\');      

    const auto escaped = backslash >> _char;
  
    const auto chars = kleene(escaped | not_quote) >>= [](auto chars) {
      std::string value(chars.begin(), chars.end());
      return unit(sexpr(std::move(value)));
    };
  
    const auto string = token(quote) >> chars >>= drop(quote); 
  
    const auto cast = [](auto value) { return sexpr(value); };
  
    const auto number = longest(map(_long, cast),
                                map(_double, cast));

    const auto space = pred(std::isspace);
  
    const auto atom = debug("atom", number | string);
  
    const auto list = [=](auto parser) {
      auto inner = map(((parser % space) | unit(sexpr::list{})), cast);
      return lparen >> inner >>= drop(rparen);
    };
  
    static const any<sexpr> expr = atom | list(ref(expr));
    return ref(expr);
  }
  
};



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


