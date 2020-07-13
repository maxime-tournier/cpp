#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "parser.hpp"
#include "variant.hpp"


struct sexpr: variant<long, double, std::string, std::deque<sexpr>> {
  using sexpr::variant::variant;
  using list = std::deque<sexpr>;

  friend std::ostream& operator<<(std::ostream& out, const sexpr& self) {
    match(self,
          [&](const auto& value) { out << value; },
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

    const auto lparen = token(single('('));
    const auto rparen = token(single(')'));

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
  
    const auto atom = number | string;
  
    const auto list = [=](auto parser) {
      auto inner = map(((parser % space) | unit(sexpr::list{})), cast);
      return lparen >> inner >>= drop(rparen);
    };

    const auto expr = fix<sexpr>([=](auto self) { return atom | list(self); });
    return expr;
  }
  
};




#endif
