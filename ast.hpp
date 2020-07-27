#ifndef AST_HPP
#define AST_HPP

#include "variant.hpp"
#include "symbol.hpp"
#include "fix.hpp"
#include "list.hpp"

#include <string>

struct sexpr;

namespace ast {

struct lit: variant<long, double, std::string> {
  using lit::variant::variant;
};

struct abs;
struct app;
struct var;
struct let;

struct attr;

struct expr: variant<lit, var, abs, app, let, attr> {
  using expr::variant::variant;
};

expr check(const sexpr&);

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

struct def {
  symbol name;
  expr value;
};

struct let {
  list<def> defs;
  expr body;
};

struct attr {
  expr arg;
  symbol name;
};

// TODO recursion schemes?

}


#endif
