#ifndef SEXPR_HPP
#define SEXPR_HPP

#include "parser.hpp"
#include "variant.hpp"
#include "symbol.hpp"
#include "list.hpp"
#include "fix.hpp"

template<class T>
struct Attrib {
  T arg;
  symbol name;
};

template<class T>
struct Sexpr: variant<long, double, std::string, symbol, Attrib<T>, list<T>> {
  using Sexpr::variant::variant;

  // TODO map
};

struct sexpr;
using attrib = Attrib<sexpr>;

struct sexpr: fix<Sexpr, sexpr> {
  using sexpr::fix::fix;
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

  parser::range source;
  
  static parser::monad<sexpr> parse();
};




#endif
