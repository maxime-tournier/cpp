#ifndef AST_HPP
#define AST_HPP

#include "variant.hpp"
#include "symbol.hpp"

#include <string>

namespace ast {

struct lit: variant<long, double, std::string> {
  using lit::variant::variant;
};

struct abs;
struct app;
struct var;

struct expr: variant<lit, var, abs, app> {
  using expr::variant::variant;
};

struct var {
  symbol name;
};

struct abs {
  var arg;
  expr body;
};

struct app {
  expr func;
  expr arg;
};





}


#endif