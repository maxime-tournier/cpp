#include "type.hpp"
#include "ast.hpp"
#include "either.hpp"
#include "unit.hpp"
#include "hamt.hpp"
#include "common.hpp"

#include <functional>
#include <stack>
#include <sstream>
#include <algorithm>
#include <sstream>

#include "sexpr.hpp"


////////////////////////////////////////////////////////////////////////////////
#ifdef SLIP_DEBUG
#include <iostream>

template<class... Args>
static void debug(const Args&... args) {
  int i = 0;
  const int expand[] = {((std::clog << (i++ ? " " : "") << args), 0)...};
  (void)expand;
  
  std::clog << std::endl;
}

#else

#define debug(...)

#endif



////////////////////////////////////////////////////////////////////////////////
namespace type {

const kind term = kind_constant("*");
const kind row = kind_constant("@");
const kind tag = kind_constant("#");

const mono func = type_constant("->", term >>= term >>= term, true);

const mono boolean = type_constant("bool");
const mono integer = type_constant("int");
const mono number = type_constant("num");
const mono string = type_constant("str");

const mono record = type_constant("record", row >>= term);
const mono sum = type_constant("sum", row >>= term);

const mono empty = type_constant("empty", row);

const mono box = type_constant("box", tag >>= term >>= term);

const mono lst = type_constant("list", term >>= term);
const mono ty = type_constant("type", term >>= term);


struct type_error: std::runtime_error {
  type_error(std::string what): std::runtime_error("type error: " + what) { }
};

////////////////////////////////////////////////////////////////////////////////
// kinds
////////////////////////////////////////////////////////////////////////////////

kind kind::operator>>=(kind other) const {
  return ctor{*this, other};
}

bool kind::operator==(kind other) const {
    if(type() != other.type()) {
      return false;
    }
    
    return match(*this,
                 [&](ctor lhs) {
                   return std::tie(lhs.from, lhs.to) ==
                     std::tie(other.get<ctor>().from, other.get<ctor>().to);
                 },
                 [&](kind_constant lhs) {
                   return lhs == other.get<kind_constant>();
                 });
}

std::string kind::show() const {
  return match(*this,
               [&](kind_constant self) -> std::string {
                 return self->name.repr;
               },
               [&](ctor self) {
                 return self.from.show() + " -> " + self.to.show();
               });
}



struct kind_error: std::runtime_error {
  kind_error(std::string what): std::runtime_error("kind error: " + what) { }
};

////////////////////////////////////////////////////////////////////////////////
mono mono::operator>>=(mono to) const {
  return func(*this)(to);
}

mono mono::operator()(mono arg) const {
  return match(kind(),
               [&](ctor self) -> mono {
                 if(!(self.from == arg.kind())) {
                   throw kind_error("expected: " + self.from.show() +
                                    ", found: " + arg.kind().show());
                 }
                 
                 return app{*this, arg};
               },
               [](auto) -> mono {
                 assert(false);
                 throw kind_error("type constructor expected");
               });
}


static std::string var_id(var self) {
  std::stringstream ss;
  ss << self.get();
  std::string s = ss.str();
  
  return s.substr(s.size() - 6);
};


std::string mono::show(repr_type repr) const {
  struct result {
    const std::string value;
    const bool flip;
    const bool parens;
    result(std::string value, bool flip = false, bool parens = false):
      value(value), flip(flip), parens(parens) {}
  };

  // TODO handle parens
  return cata(*this, [&](Mono<result> self) {
    return match(self,
                 [](type_constant self) -> result {
                   return {self->name.repr, self->flip};
                 },
                 [&](var self) -> result {
                   const auto it = repr.find(self);
                   if(it == repr.end()) {
                     return "<var:" + var_id(self) + ">";
                   }

                   return it->second;
                 },
                 [](App<result> self) -> result {
                   const std::string ctor = self.ctor.parens ?
                     parens(self.ctor.value) : self.ctor.value;
                   
                   const std::string arg = self.arg.parens ?
                     parens(self.arg.value) : self.arg.value;
                   
                   if(self.ctor.flip) {
                     return {arg + " " + ctor, false, false};
                   } else {
                     return {ctor + " " + arg, false, true};
                   }
                 });
  }).value;
}


struct kind mono::kind() const {
  return cata(*this, [](Mono<struct kind> self) {
    return match(self,
                 [](type_constant self) { return self->kind; },
                 [](var self) { return self->kind; },
                 [](App<struct kind> self) {
                   return self.ctor.get<ctor>().to;
                 });
  });
}


template<class T>
static list<T> unique(list<T> source) {
  std::set<T> prune;
  for(auto& it: source) {
    prune.insert(it);
  }
  
  return make_list(prune.begin(), prune.end());
}

list<var> mono::vars() const {
  return unique(cata(*this, [](Mono<list<var>> self) {
    return match(self,
                 [](var self) { return self %= list<var>{}; },
                 [](App<list<var>> self) {
                   return concat(self.ctor, self.arg);
                 },
                 [](type_constant self) { return list<var>{}; });
  }));
}


bool mono::operator==(mono other) const {
  if(this->type() != other.type()) {
    return false;
  }

  return match(*this,
               [&](var self) { return self == other.get<var>(); },
               [&](type_constant self) {
                 return self == other.get<type_constant>();
               },
               [&](app self) {
                 return self.ctor == other.get<app>().ctor
                   && self.arg == other.get<app>().arg;
               });
  
}

////////////////////////////////////////////////////////////////////////////////
// polytypes
////////////////////////////////////////////////////////////////////////////////

mono poly::body() const {
  return cata(*this, [](Poly<mono> self) {
    return match(self,
                 [](Forall<mono> self) { return self.body; },
                 [](mono self) { return self; });
  });
}



list<var> poly::bound() const {
  return cata(*this, [](Poly<list<var>> self) {
    return match(self,
                 [](Forall<list<var>> self) {
                   return self.arg %= self.body;
                 }, 
                 [](mono self) { return list<var>{}; });
  });
}

static std::string var_name(std::size_t index, kind k) {
  std::string res;

  static constexpr std::size_t basis = 26;
  
  do {
    const std::size_t digit = index % basis;
    res += 'a' + digit;
    index /= basis;
  } while(index);

  res += "'";
  
  std::reverse(res.begin(), res.end());
  
  if(k == row) {
    res += "...";
  }

  return res;
}

static repr_type label(list<var> vars, repr_type repr={}) {
  for(auto var: vars) {
    repr.emplace(var, var_name(repr.size(), var->kind));
  }
  
  return repr;
}

std::string poly::show(repr_type repr) const {
  return body().show(label(bound()));
}


} // namespace type


////////////////////////////////////////////////////////////////////////////////

namespace type {


static std::string show(mono self) {
  static repr_type repr;
  repr = label(self.vars(), std::move(repr));
  return self.show(repr);
}


////////////////////////////////////////////////////////////////////////////////
// substitutions
////////////////////////////////////////////////////////////////////////////////

class substitution {
  using table_type = hamt::map<const var_info*, mono>;
  table_type table;

  substitution(table_type table): table(table) { }
public:
  substitution() = default;
  
  substitution link(var a, mono ty) const {
    // TODO compress var => var
    return {table.set(a.get(), std::move(ty))};
  }
  
  mono operator()(mono ty) const {
    return cata(ty, [&](Mono<mono> self) {
      return match(self,
                   [&](var self) -> mono {
                     if(auto res = table.find(self.get())) {
                       // keep substituting
                       return (*this)(*res);
                     }
                     return self;
                   },
                   [](auto self) -> mono { return self; });                   
    });
  }

};


////////////////////////////////////////////////////////////////////////////////
// context
////////////////////////////////////////////////////////////////////////////////

struct context {
  std::size_t depth = 0;
  hamt::map<symbol, poly> locals;

  var fresh(struct kind kind=term) const {
    return var(depth, kind);
  }
  
  mono instantiate(poly p) const {
    const substitution sub =
        foldr(p.bound(), substitution{}, [&](var a, substitution sub) {
          return sub.link(a, fresh(a->kind));
        });

    return sub(p.body());
  };


  // quantify unbound variables
  list<var> quantify(list<var> vars) const {
    return foldr(vars, list<var>{}, [&](var a, list<var> rest) {
      if(a->depth < depth) {
        // bound somewhere in enclosing scope
        return rest;
      } else {
        // can be generalized (TODO assert it's not in locals just in case)
        // assert(a->depth == depth);
        return a %= rest;
      }
    });
  }


  // note: m must be fully substituted
  poly generalize(mono ty) const {
    const auto foralls = quantify(ty.vars());

    // replace quantified variables with fresh variables just in case
    substitution sub;
    
    const auto freshes = map(foralls, [&](var a) {
      const auto res = fresh(a->kind);
      sub = sub.link(a, res);
      return res;
    });

    return foldr(freshes, poly(sub(ty)), [](var a, poly p) -> poly {
      return forall{a, p};
    });
  }

  context scope() const {
    return {depth + 1, locals};
  }
  
  void def(symbol name, poly p) {
    locals = locals.set(name, std::move(p));
  }
};




////////////////////////////////////////////////////////////////////////////////
// inference monad
////////////////////////////////////////////////////////////////////////////////

struct state {
  context ctx;
  substitution sub;
  hamt::array<mono> types;
};

template<class T>
struct success {
  using value_type = T;
  T value;
  struct state state;
};

template<class T>
static success<T> make_success(T value, state s) {
  return {value, s};
}


using call_stack = std::stack<std::string>;

template<class T>
using result = either<call_stack, success<T>>;

template<class T>
using monad = std::function<result<T>(state)>;

template<class M>
using value = typename std::result_of<M(state)>::type::value_type::value_type;

template<class T>
static auto pure(T value) {
  return [value](state s) -> result<T> {
    return make_success(value, s);
  };
}

template<class M, class Func, class T=value<M>>
static auto bind(M self, Func func) {
  return [=](state s) {
    return self(s) >>= [&](const success<T>& self) {
      return func(self.value)(self.state);
    };
  };
}

template<class M, class Func, class T=value<M>>
static auto operator>>=(M self, Func func) {
  return bind(self, func);
}


template<class LHS, class RHS, class L=value<LHS>, class R=value<RHS>>
static auto operator>>(LHS lhs, RHS rhs) {
  return lhs >>= [=](auto&&) { return rhs; };
}


template<class MA, class Func, class A=value<MA>>
static auto map(MA self, Func func) {
  return self >>= [=](A value) {
    return pure(func(value));
  };
}

template<class MA, class Func, class A=value<MA>>
static auto operator|=(MA self, Func func) {
  return map(self, func);
}


template<class MA, class MB, class A=value<MA>, class B=value<MB>>
static auto coprod(MA ma, MB mb) {
  return [=](state s) {
    if(auto res = ma(s)) {
      return res;
    }
    
    return mb(s);
  };
}


template<class MA, class MB, class A=value<MA>, class B=value<MB>>
static auto operator|(MA ma, MB mb) {
  return coprod(ma, mb);
}


template<class M, class T=value<M>>
static monad<list<value<M>>> sequence(list<M> items) {
  if(!items) {
    return pure(list<T>{});
  } else return items->head >>= [=](T head) {
    return sequence(items->tail) >>= [=](list<T> tail) {
      return pure(head %= tail);
    };
  };
}

////////////////////////////////////////////////////////////////////////////////

template<class T>
static auto fail(std::string what) {
  return [what](state) -> result<T> {
    call_stack res;
    res.push(what);
    return res;
  };
}


static auto upgrade(mono ty, var target) {
  const std::size_t max = target->depth;
  return [=](state s) -> result<unit> {
    const auto vars = ty.vars();
    for(auto v: vars) {
      if(v == target) {
        const auto repr = label(vars);
        return fail<unit>("type variable " + quote(mono(target).show(repr)) +
                          " occurs in type " + quote(ty.show(repr)))(s);
      }
      
      if(v->depth > max) {
        const mono target = var(max, v->kind);
        debug("upgrade:", show(v), "==", show(target));
        s.sub = s.sub.link(v, target);
      }
    }
    
    return make_success(unit{}, s);
  };
}


static auto link(var from, mono to) {
  return [=](state s) -> result<unit> {
    debug("> link:", show(from), "==", show(to));
    
    s.sub = s.sub.link(from, to);
    return make_success(unit{}, s);
  };
}

static auto substitute(mono ty) {
  return [=](state s) -> result<mono> {
    return make_success(s.sub(ty), s);
  };
}



template<class M, class T=value<M>>
static auto scope(M m) {
  return [m=std::move(m)](state s) -> result<T> {

    state inner = s;
    inner.ctx = s.ctx.scope();

    // update substitution from inner scope, drop context
    return m(inner) >>= [=](success<T> self) -> result<T> {
      self.state.ctx = s.ctx;
      return make_success(self.value, self.state);
    };
  };
}

static auto def(symbol name, poly p) {
  return [=](state s) -> result<unit> {
    s.ctx.def(name, p);         // FIXME return new context?
    return make_success(unit{}, s);
  };
};

static auto generalize(mono ty) {
  return [=](state s) -> result<poly> {
    return make_success(s.ctx.generalize(ty), s);
  };
}


static auto fresh(kind k=term) {
  return [=](state s) -> result<var> {
    return make_success(s.ctx.fresh(k), s);
  };
}


static auto find(symbol name) {
  return [=](state s) -> result<poly> {
    if(auto poly = s.ctx.locals.find(name)) {
      return make_success(*poly, s);
    }
    
    return fail<poly>("unbound variable: " + quote(name))(s);
  };
};

static auto instantiate(poly p) {
  return [=](state s) -> result<mono> {
    return make_success(s.ctx.instantiate(p), s);
  };
};


static auto decorate(ast::expr e, mono t) {
  return [=](state s) -> result<unit> {
    s.types = s.types.set(e.id(), mono(t));
    return make_success(unit{}, s);
  };
}


template<class Lazy,
         class M,
         class T=value<M>>
static auto with_context(Lazy lazy, M m) {
  return [=](state s) {
    return match(m(s),
                 [](success<T> self) -> result<T> { return self; },
                 [=](call_stack error) -> result<T> {
                   error.push(lazy());
                   return error;
                 });
  };
};

////////////////////////////////////////////////////////////////////////////////
// unification
////////////////////////////////////////////////////////////////////////////////

static monad<unit> unify(mono lhs, mono rhs);
static monad<unit> unify_vars(mono lhs, mono rhs);

static monad<unit> unify_app_terms(app lhs, app rhs);
static monad<unit> unify_app_rows(app lhs, app rhs);



static const auto app_match = [](mono ty, auto cont) {
  return match(ty,
               [&](app self) {
                 return cont(self.ctor, self.arg);
               },
               [&](auto) {
                 return fail<unit>("not a type application");
               });
};

static const auto type_constant_match = [](mono ty, auto cont) {
  return match(ty,
               [&](type_constant self) {
                 return cont(self);
               },
               [&](auto) {
                 return fail<unit>("not a type constant");
               });
};


static const auto row_match = [](mono ty, auto cont) {
  return app_match(ty, [&](mono outer, mono tail) {
    return app_match(outer, [&](mono ctor, mono arg) {
      return type_constant_match(ctor, [&](type_constant ctor) {
        // TODO check that kind is correct
        return cont(ctor->name, arg, tail);
      });
    });      
  });
};

// row extension type constructor ::: * -> @ -> @
static mono ext(symbol name);


static monad<unit> unify_sub(mono lhs, mono rhs) {
  return substitute(lhs) >>= [=](mono lhs) {
    return substitute(rhs) >>= [=](mono rhs) {
      return unify(lhs, rhs);
    };
  };
}


// TODO nicer errors
static monad<unit> unify_app_rows(app lhs, app rhs) {
  return row_match(lhs, [&](symbol lattr, mono lty, mono ltail) -> monad<unit> {
    return row_match(rhs, [&](symbol rattr, mono rty, mono rtail) -> monad<unit> {
      if(lattr == rattr) {
        // labels match: unify value types
        return unify(lty, rty) >> unify_sub(ltail, rtail);
      } else {
        // labels mismatch: look for labels in each tails
        return fresh(row) >>= [=](mono ldummy) {
          return fresh(row) >>= [=](mono rdummy) {
            return unify(ltail, ext(rattr)(rty)(rdummy)) >>
                   unify_sub(rtail, ext(lattr)(lty)(ldummy));
            // TODO proceed with tails?
          };
        };
      }        
    });
  });
}

// note: lhs/rhs must be fully substituted
static monad<unit> unify(mono lhs, mono rhs) {
  if(lhs.kind() != rhs.kind()) {
    repr_type repr;
    repr = label(lhs.vars(), std::move(repr));
    repr = label(rhs.vars(), std::move(repr));  
    
    return fail<unit>("cannot unify types " + quote(lhs.show(repr)) +
                      " and " + quote(rhs.show(repr)));
  }

  const auto trace = [=](state s) -> result<unit> {
    debug("unifying:", show(lhs), "==", show(rhs));
    return make_success(unit{}, s);
  };
  
  const auto kind = lhs.kind();

  const auto lapp = lhs.cast<app>();
  const auto rapp = rhs.cast<app>();
  
  // both applications
  if(lapp && rapp) {
    if(kind == row) {
      return trace >> unify_app_rows(*lapp, *rapp);
    }

    return trace >> unify_app_terms(*lapp, *rapp);
  }    

  const auto lcst = lhs.cast<type_constant>();
  const auto rcst = rhs.cast<type_constant>();

  // both constants
  if(lcst && rcst) {
    if(*lcst == *rcst) {
      return trace >> pure(unit{});
    }
  }
  
  // variables
  return trace >> unify_vars(lhs, rhs);
}


static monad<unit> unify_vars(mono lhs, mono rhs) {
   assert(lhs.kind() == rhs.kind());
  
  const auto lvar = lhs.cast<var>();
  const auto rvar = rhs.cast<var>();  
  
  // both variables
  if(lvar && rvar) {
    const var target(std::min((*lvar)->depth,
                              (*rvar)->depth), lhs.kind());
    // TODO create target *inside* monad?
    return link(*lvar, target) >> link(*rvar, target);
  } else if(lvar) {
    return link(*lvar, rhs) >> upgrade(rhs, *lvar);
  } else if(rvar) {
    return link(*rvar, lhs) >> upgrade(lhs, *rvar);    
  }

  repr_type repr;
  repr = label(lhs.vars(), std::move(repr));
  repr = label(rhs.vars(), std::move(repr));  
  
  return fail<unit>("cannot unify types " + quote(lhs.show(repr)) +
                    " and " + quote(rhs.show(repr)));  
}


static monad<unit> unify_app_terms(app lhs, app rhs) {
  return unify(lhs.ctor, rhs.ctor) >> unify_sub(lhs.arg, rhs.arg);
};




////////////////////////////////////////////////////////////////////////////////
static monad<mono> infer(ast::expr e);

static monad<mono> infer(ast::lit self) {
  return pure(match(self,
                    [](long) { return integer; },
                    [](double) { return number; },
                    [](std::string) { return string; },
                    [](bool) { return boolean; }));
};


static monad<mono> infer(ast::var self) {
  return find(self.name) >>= [](poly p) {
    return instantiate(p) >>= substitute;
  };
};



static monad<mono> check_type(mono type) {
  return (fresh() >>= [=](mono arg) {
    return unify(ty(arg), type) >> substitute(arg); }) |
         (fresh() >>= [=](mono arg) {
           return fresh() >>= [=](mono ctor) {
             return unify(ty(arg) >>= ctor, type) >> substitute(ctor) >>=
               [=](mono ctor) {
                 return check_type(ctor);
               };
           };
         });
}


template<class Func, class Result=typename std::result_of<Func(symbol, mono)>::type>
static auto map_row(mono row, Func func) -> list<Result> {
  return match(row,
               [&](app self) {
                 const mono tail = self.arg;
                 const app ctor = self.ctor.get<app>();
                 const mono arg = ctor.arg;
                 const symbol name = ctor.ctor.get<type_constant>()->name;
                 
                 return func(name, arg) %= map_row(tail, func);
               },
               [](type_constant self) {
                 if(self == empty.get<type_constant>()) {
                   return list<Result>();
                 }

                 throw std::logic_error("derp");
               },
               [](auto) -> list<Result> {
                 throw std::logic_error("derp");
               });
};



//   return infer(self.def) >>= [=](mono def) {
//     return fresh(row) >>= [=](mono row) {
//       return unify(def, record(row)) >> substitute(row) >>= [=](mono row) {
//         return sequence(map_row(row, [](symbol name, mono arg) {
//           return check_type(arg) |= [=](mono arg) {
//             return ext(name)(arg);
//           };
//         })) |= [](list<mono> ctors) {
//           const mono def = ty(record(foldr(ctors, empty, [](mono ctor, mono tail) {
//             return ctor(tail);
//           })));

//           // TODO we should probably prevent bound type variables

//           // TODO hence we should also accept parametric expressions like
//           // (type foo (fn (a) (record (bar (list a)))))

//           // TODO 
          
//           return def;
          
//         };
//       };
//     };
//   };
// }



static monad<poly> infer(ast::arg self) {
  return match(self,
               [](symbol self) -> monad<poly> {
                 return fresh() |= [](var res) -> poly {
                   return mono(res);
                 };
               }, 
               [](ast::annot self) -> monad<poly> {
                 return fresh() >>= [=](mono body) {
                   return scope(fresh(tag) >>= [=](var label) {
                     const mono annot = box(label)(body);
                     return infer(self.type) >>= [=](mono reified) {
                       return check_type(reified) >>= [=](mono type) {
                         return unify(annot, type) >> substitute(annot) >>=
                                generalize;
                       };
                     };
                   });
                 };
               });
}



static monad<mono> infer_abs(ast::arg arg, monad<mono> body) {
  return infer(arg) >>= [=](poly from) {
    return scope(def(arg.name(), from) >> body) >>= [=](mono to) {
      return instantiate(from) >>= [=](mono from) {
        return substitute(from >>= to);
      };
    };
  };
}

static monad<mono> infer(ast::abs self) {
  // TODO handle nullary apps  
  return foldr(self.args, infer(self.body), infer_abs);
};






static monad<type_constant> constructor(mono ty) {
  return match(ty,
               [](type_constant self) -> monad<type_constant> {
                 return pure(self);
               },
               [](app self) { return constructor(self.ctor); },
               [](var self) -> monad<type_constant> {
                 return fail<type_constant>("type constructor is not a constant");
               });
}

static monad<mono> check_annot(mono offered) {
  // match annotated type
  return fresh(tag) >>= [=](mono label) {
    return fresh() >>= [=](mono body) {
      return ((unify(box(label)(body), offered) >> substitute(label)) >>= generalize)
        // check whether label is generalized
        >>= [=](poly label) {
          return match(label,
                       [=](forall) -> monad<mono> {
                         // label is generalizable: either type is used-provided
                         // or has no sharing through context
                         return substitute(body);
                       },
                       [](mono) -> monad<mono> { 
                         // type has sharing without type annotation, cannot open
                         return fail<mono>("cannot open type");
                       });
        };
    };
  };
};


// try to open type
static monad<mono> open(mono offered) {
  return check_annot(offered) >>= [=](mono offered) {
    // TODO find a way to put opening type in box instead
    return constructor(offered) >>= [=](type_constant ctor) {
      return instantiate(ctor->open(ctor)) >>= [=](mono open) {
        return fresh() >>= [=](mono res) {
          return unify(offered >>= res, open) >> substitute(res);
        };
      };
    };
  };
};


static monad<unit> subsume(mono requested, mono offered) {
  // try standard unification, fallback to opening offered type if possible
  return unify(requested, offered) | (open(offered) >>= [=](mono opened) {
    debug("opened", show(offered), "as", show(opened));    
    return unify(requested, opened);
  });
}

static monad<mono> infer_app(monad<mono> func, ast::expr arg) {
  return func >>= [=](mono func) {
    return infer(arg) >>= [=](mono off) {
      return substitute(func) >>= [=](mono func) {
        return fresh() >>= [=](mono ret) {
          return fresh() >>= [=](mono req) {
            // funmatch
            return (unify(req >>= ret, func) >> substitute(req)) >>= [=](mono req) {
              return subsume(req, off) >> substitute(ret);
            };
          };
        };
      };
    };
  };
}
  


static monad<mono> infer(ast::app self) {
  return foldl(infer(self.func), self.args, infer_app);
};



static monad<mono> infer_def(ast::def def, monad<mono> tail) {
  return (infer(def.value) >>= check_type) >>= [=](mono type) {
    return tail |= [=](mono tail) {
      return ext(def.name)(type)(tail);
    };
  };
}

static mono module(ast::module_type type) {
  switch(type) {
  case ast::STRUCT: return record;
  case ast::UNION: return sum;    
  }
}


static monad<mono> infer(ast::module self) {
  const monad<mono> init = pure(empty);
  const monad<mono> defs = foldr(self.defs, init, infer_def) |= [=](mono row) {
    return ty(module(self.type)(row));
  };
  
  return foldr(self.sig.args, defs, infer_abs);
};



static monad<mono> infer(ast::cond self) {
  return infer(self.pred) >>= [=](mono pred) {
    return (unify(pred, boolean) >> infer(self.conseq)) >>= [=](mono conseq) {
      return infer(self.alt) >>= [=](mono alt) {
        return unify(conseq, alt) >> substitute(conseq);
      };
    };
  };
}


static monad<mono> infer(ast::record self) {
  const monad<mono> init = pure(empty);
  return foldr(self.attrs, init,
               [](ast::def self, monad<mono> tail) -> monad<mono> {
    return infer(self.value) >>= [=](mono ty) {
      return tail |= [=](mono rows) -> mono {
        return ext(self.name)(ty)(rows);
      };
    };
  }) |= [](mono rows) {
    return record(rows);
  };
}



static monad<mono> infer(ast::let self) {
  // assign fresh vars to defs
  const auto create_vars = sequence(map(self.defs, [](ast::def) {
    return fresh();
  }));

  // push let scope, create vars
  return scope(create_vars >>= [=](auto vars) {

    // populate defs scope (monomorphic)
    const auto populate_defs =
      sequence(zip(self.defs, vars, [](ast::def self, var a) {
        return def(self.name, mono(a));
      }));

    // infer + unify defs with vars
    const auto infer_defs =
      sequence(zip(self.defs, vars, [](ast::def self, var a) {
        return infer(self.value) >>= [=](mono ty) {
          return substitute(a) >>= [=](mono a) {
            // TODO detect useless definitions
            return unify(a, ty);
          };
        };
      }));

    // generalize vars + populate body scope (polymorphic)
    const auto populate_body =
      sequence(zip(self.defs, vars, [](ast::def self, var a) {
        return substitute(a) >>= [=](mono ty) {
          return generalize(ty) >>= [=](poly p) {
            return def(self.name, p);
          };
        };
      }));
    
    return scope(populate_defs >> infer_defs) >>
      populate_body >> infer(self.body);
  });
};

static auto infer_choice(mono res) {
  return [=](ast::choice self, monad<mono> tail) -> monad<mono> {
    return infer_abs(self.arg, infer(self.value)) >>= [=](mono sig) {
      return fresh() >>= [=](mono arg) {
        return substitute(res) >>= [=](mono res) {
          return unify(arg >>= res, sig) >> substitute(arg) >>= [=](mono arg) {
            // note: tail still needs substitution
            return tail |= [=](mono tail) {
              return ext(self.name)(arg)(tail);
            };
          };
        };
      };
    };
  };
}

static monad<mono> infer(ast::pattern self) {
  return infer(self.arg) >>= [=](mono arg) {  
    return fresh() >>= [=](mono res) {
      const monad<mono> init = pure(empty);
      return (foldr(self.choices, init, infer_choice(res)) >>= substitute)
        >>= [=](mono row) {
          return substitute(arg) >>= [=](mono arg) {
            return unify(arg, sum(row)) >> substitute(res);
          };
        };
      };
  };
}

// static monad<mono> infer(ast::open self) {
//   // attempt to extract ctor info
//   return infer(self.arg) >>= [=](mono arg) {
//     return constructor(arg) >>= [=](type_constant ctor) -> monad<mono> {

//       if(!ctor->open) {
//         // TODO use some default opening scheme instead?
//         return fail<mono>("type " + quote(arg.show()) + " cannot be opened");
//       }

//       // instantiate open type and unify with arg
//       return instantiate(ctor->open(ctor)) >>= [=](mono open) {
//         return fresh() >>= [=](mono result) {
//           return unify(arg >>= result, open) >> substitute(result);
//         };
//       };
//     };
//   };
// }

// row extension type constructor ::: * -> @ -> @
static mono ext(symbol name) {
  static std::map<symbol, type_constant> table;
  
  const auto it = table.find(name);
  if(it != table.end()) return it->second;

  type_constant result(name, term >>= row >>= row);

  // cache
  table.emplace(name, result);
  
  return result;  
}


// attribute projection type: forall a.rho.{name: a; rho} -> a
static poly proj(symbol name) {
  const var a(0ul, term), rho(0ul, row);
  const mono body = record(ext(name)(a)(rho)) >>= a;
  
  return forall{a, forall{rho, body}};
};



static monad<mono> infer(ast::attr self) {
  return infer_app(instantiate(proj(self.name)), self.arg);
};

template<class T>
static monad<mono> infer(T) {
  return fail<mono>("unimplemented: " + std::string(typeid(T).name()));
}


monad<mono> infer(ast::expr e) {
  return with_context([=] { 
    return "when typing expression " + std::string(e.source->source.first,
                                                   e.source->source.last);
  },  match(e, [=](auto self) -> monad<mono> {
      return infer(self) >>= [=](mono result) {
        return decorate(e, result) >> pure(result);
      };
    }));
}


// static monad<mono> infer(ast::expr e) {
//   return cata(e, [](ast::Expr<monad<mono>> self) -> monad<mono> {
//       return match(self, [](auto self) {
//         return infer(self);
//       });
//     });
// }



// user-facing api
std::shared_ptr<context> make_context() {
  auto res = std::make_shared<context>();

  {
    const mono a = res->fresh();
    res->def("cons", res->generalize(a >>= lst(a) >>= lst(a)));
  }


  {
    const mono a = res->fresh();
    res->def("nil", res->generalize(lst(a)));
  }


  {
    const mono module =
      type_constant("module", term >>= term, false, [=](mono self) {
      const mono a = res->fresh();
      const mono b = res->fresh();
      return res->generalize(self(a) >>= record(ext("nil")(lst(a))
                                                (ext("map")((a >>= b) >>= lst(a) >>= lst(b))
                                                 (empty))));
    });

    const mono e = res->fresh(tag);
    const mono a = res->fresh();
    res->def("module", res->generalize(ty(a) >>= ty(box(e)(module(a)))));

    {
      const mono e = res->fresh(tag);
      const mono a = res->fresh();
      const mono b = res->fresh();      

      res->def("share", res->generalize(box(e)(module(a)) >>= b >>= integer));
    }

    
  }


  res->def("eq", integer >>= integer >>= boolean);
  res->def("add", integer >>= integer >>= integer);
  res->def("sub", integer >>= integer >>= integer);
  res->def("mul", integer >>= integer >>= integer);      
  

  res->def("int", ty(integer));
  {
    const mono a = res->fresh();    
    res->def("list", res->generalize(ty(a) >>= ty(lst(a))));
  }

  {
    const mono a = res->fresh();
    const mono b = res->fresh();        
    res->def("arrow", res->generalize(ty(a) >>= ty(b) >>= ty(a >>= b)));  
  }

  {
    const mono a = res->fresh();
    res->def("type", res->generalize(ty(a) >>= ty(ty(a)))); // oh wow
  }

  
  return res;
}



poly infer(std::shared_ptr<context> ctx, const ast::expr& e, hamt::array<mono>* types) {
  substitution sub;
  state s = {*ctx, sub};
  
  const mono ty = match(infer(e)(s),
                        [=](success<mono> self) {
                          // update context/types
                          *ctx = self.state.ctx;
                          
                          if(types) {
                            // store substituted types
                            self.state.types.iter([&](std::size_t i, mono t) {
                              *types = types->set(i, self.state.sub(t));
                            });
                          }
                          
                          return self.value;
                        },
                        [](call_stack error) -> mono {
                          std::stringstream ss;
                          while(!error.empty()) {
                            ss << error.top();
                            error.pop();
                            if(!error.empty()) {
                              ss << '\n';
                            }
                          }
                          throw std::runtime_error(ss.str());
                        });
  
  return ctx->generalize(ty);
}
 
} // namespace type

