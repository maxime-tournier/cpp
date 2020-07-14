#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "parser.hpp"
#include "variant.hpp"
#include "symbol.hpp"
#include "list.hpp"

struct sexpr: variant<long, double, std::string, symbol, list<sexpr>> {
  using sexpr::variant::variant;
  using list = list<sexpr>;

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

    const auto first = pred(std::isalpha);
    const auto next = pred(std::isalnum);
    
    const auto symbol = first >>= [=](char first) {
      return kleene(next) >>= [=](auto nexts) {
        nexts.emplace_front(first);
        std::string repr(nexts.begin(), nexts.end());
        struct symbol s(repr.c_str());
        return unit(sexpr(s));
      };
    };
    
    const auto atom = number | string | symbol;
  
    const auto list = [=](auto parser) {
      auto inner = ((parser % space) | unit(std::deque<sexpr>{})) |= [](auto items) {
        // TODO use move iterator
        return sexpr(make_list(items.begin(), items.end()));
      };
      
      return lparen >> inner >>= drop(rparen);
    };

    const auto expr = fix<sexpr>([=](auto self) { return atom | list(self); });
    return expr;
  }
  
};




#endif
