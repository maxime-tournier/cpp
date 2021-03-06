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

struct var {
  symbol name;
};

template<class E>
struct Annot {
  symbol name;
  E type;
};

template<class E>
struct Arg: variant<symbol, Annot<E>> {
  using Arg::variant::variant;

  symbol name() const {
    return match(*this,
                 [](symbol self) { return self; },
                 [](Annot<E> self) { return self.name; });
  }

  template<class F, class G=std::result_of_t<F(E)>>
  friend Arg<G> map(Arg self, const F& f) {
    return match(self,
                 [](symbol self) -> Arg<G> {
                   return self;
                 },
                 [&](Annot<E> self) -> Arg<G> {
                   return Annot<G>{self.name, f(self.type)};
                 });
  }
};



template<class E>
struct Abs {
  list<Arg<E>> args;
  E body;
};

template<class E>
struct App {
  E func;
  list<E> args;
};

template<class E>
struct Def {
  symbol name;
  E value;

  template<class F, class G=std::result_of_t<F(E)>>
  friend Def<G> map(Def self, const F& f) {
    return {self.name, f(self.value) };
  }
};


template<class E>
struct Let {
  list<Def<E>> defs;
  E body;

  template<class F, class G=std::result_of_t<F(E)>>
  friend Let<G> map(Let self, const F& f) {
    return {map(self.defs, [&](Def<E> self) {
      return Def<G>{self.name, f(self.value)};
    }, f(self.body))};
  }
  
};

template<class E>
struct Cond {
  E pred;
  E conseq;
  E alt;
};

template<class E>
struct Record {
  list<Def<E>> attrs;
};


template<class E>
struct Choice {
  symbol name;
  Arg<E> arg;
  E value;
};


template<class E>
struct Pattern {
  E arg;
  list<Choice<E>> choices;
  // TODO fallback?
};

template<class E>
struct Attr {
  symbol name;
  E arg;
};

enum module_type {
  STRUCT,
  UNION,
};

template<class E>
struct Sig {
  symbol name;
  list<Arg<E>> args;

  template<class F, class G=std::result_of_t<F(E)>>
  friend Sig<G> map(Sig self, const F& f) {
    return {self.name, map(self.args, [=](Arg<E> arg) {
      return map(arg, f);
    })};
  }
  
};


template<class E>
struct Module {
  module_type type;
  Sig<E> sig;
  list<Def<E>> defs;

  template<class F, class G=std::result_of_t<F(E)>>
  friend Module<G> map(Module self, const F& f) {
    return {self.type, map(self.sig, f), map(self.defs, [=](Def<E> def) {
      return map(def, f);
    })};
  }
};


template<class E>
struct Expr: variant<lit, var, Abs<E>, App<E>, Let<E>,
                     Cond<E>,
                     Record<E>, Attr<E>,
                     Pattern<E>,
                     Module<E>> {
  using Expr::variant::variant;

  template<class F, class G=std::result_of_t<F(E)>>
  friend Expr<G> map(const Expr& self, const F& f) {
    return match(self,
                 [](lit self) -> Expr<G> { return self; },
                 [](var self) -> Expr<G> { return self; },
                 [&](Abs<E> self) -> Expr<G> {
                   return Abs<G>{map(self.args, [&](Arg<E> arg) {
                     return map(arg, f);
                   }), f(self.body)};
                 },
                 [&](App<E> self) -> Expr<G> {
                   return App<G>{f(self.func), map(self.args, f)};
                 },
                 [&](Let<E> self) -> Expr<G> {
                   return Let<G>{map(self.defs, [&](Def<E> self) {
                     return map(self, f);
                   }), f(self.body)};
                 },
                 [&](Cond<E> self) -> Expr<G> {
                   return Cond<G>{f(self.pred), f(self.conseq), f(self.alt)};
                 },
                 [&](Record<E> self) -> Expr<G> {
                   
                   return Record<G>{map(self.attrs, [&](Def<E> self) {
                     return map(self, f);
                   })};
                 },
                 [&](Attr<E> self) -> Expr<G> {
                   return Attr<G>{self.name, f(self.arg)};
                 },
                 [&](Module<E> self) -> Expr<G> {
                   return map(self, f);
                 });
                 
  }
};

struct expr: fix<Expr, expr> {
  using expr::fix::fix;

  std::shared_ptr<sexpr> source = {};
};

expr check(const sexpr&);

using annot = Annot<expr>;
using arg = Arg<expr>;
using abs = Abs<expr>;
using app = App<expr>;
using def = Def<expr>;
using let = Let<expr>;
using cond = Cond<expr>;

using sig = Sig<expr>;
using module = Module<expr>;

using record = Record<expr>;
using attr = Attr<expr>;

using pattern = Pattern<expr>;
using choice = Choice<expr>;

}


#endif
