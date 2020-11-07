#ifndef AST_HPP
#define AST_HPP

#include "variant.hpp"
#include "symbol.hpp"
#include "fix.hpp"
#include "list.hpp"

#include <string>

struct sexpr;

namespace ast {

struct lit: variant<bool, long, double, std::string> {
  using lit::variant::variant;
};

struct abs;
struct app;
struct var;
struct let;

struct cond;

struct attr;
struct record;

struct type;

struct expr: variant<lit, var, abs, app, let, cond, record, attr, type> {
  using expr::variant::variant;
};

expr check(const sexpr&);

struct var {
  symbol name;
};

struct annot {
  symbol name;
  expr type;
};

struct arg: variant<symbol, annot> {
  using arg::variant::variant;

  symbol name() const;
};

struct abs {
  list<arg> args;
  expr body;
};

struct app {
  expr func;
  list<expr> args;
};

struct def {
  symbol name;
  expr value;
};

struct let {
  list<def> defs;
  expr body;
};

struct cond {
  expr pred;
  expr conseq;
  expr alt;
};

struct record {
  list<def> attrs;
};

struct attr {
  expr arg;
  symbol name;
};

struct type {
  symbol name;
  expr def;
};



// TODO recursion schemes?

}


#endif
