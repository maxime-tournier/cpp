#include "type.hpp"
#include "ast.hpp"
#include "either.hpp"
#include "unit.hpp"

#include <functional>
#include <sstream>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////
#ifdef SLIP_DEBUG
#include <iostream>

template<class... Args>
static void debug(const Args&... args) {
  int i = 0;
  const int expand[] = {((std::clog << (i++ ? " " : "") << args), 0)...}; (void)expand;
  std::clog << std::endl;
}

#else

#define debug(...)

#endif


template<class T>
static std::string quote(const T& self) {
  std::stringstream ss;
  ss << '"' << self << '"';
  return ss.str();
}


template<class T>
static std::string parens(const T& self) {
  std::stringstream ss;
  ss << '(' << self << ')';
  return ss.str();
}



////////////////////////////////////////////////////////////////////////////////
namespace type {

const kind term = kind_constant("*");
const kind row = kind_constant("@");

const mono func = type_constant("->", term >>= term >>= term, true);

const mono boolean = type_constant("bool");
const mono integer = type_constant("int");
const mono number = type_constant("num");
const mono string = type_constant("str");

const mono record = type_constant("record", row >>= term);
const mono empty = type_constant("empty", row);

const mono lst = type_constant("list", term >>= term);

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
                   const std::string ctor = self.ctor.parens ? parens(self.ctor.value) : self.ctor.value;
                   const std::string arg = self.arg.parens ? parens(self.arg.value) : self.arg.value;
                   
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

#include "hamt.hpp"
#include <sstream>

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
  context& ctx;
  substitution& sub;
};


template<class T>
using result = either<type_error, T>;

template<class T>
using monad = std::function<result<T>(state&)>;

template<class M>
using value = typename std::result_of<M(state&)>::type::value_type;

template<class T>
static auto pure(T value) {
  return [value](state&) -> result<T> { return value; };
}

template<class MA, class Func, class A=value<MA>>
static auto bind(MA self, Func func) {
  return [self, func](state& s) {
    return self(s) >>= [&](const A& value) {
      return func(value)(s);
    };
  };
}

template<class MA, class Func, class A=value<MA>>
static auto operator>>=(MA self, Func func) {
  return bind(self, func);
}

template<class MA, class MB, class A=value<MA>, class B=value<MB>>
static auto operator>>(MA lhs, MB rhs) {
  return lhs >>= [rhs](auto&) { return rhs; };
}

static auto upgrade(mono ty, var target) {
  const std::size_t max = target->depth;
  return [=](state& s) -> result<unit> {
    const auto vars = ty.vars();
    for(auto v: vars) {
      if(v == target) {
        const auto repr = label(vars);
        return type_error("type variable " + quote(mono(target).show(repr)) +
                          " occurs in type " + quote(ty.show(repr)));
      }
      
      if(v->depth > max) {
        const mono target = var(max, v->kind);
        debug("upgrade:", show(v), "==", show(target));
        s.sub = s.sub.link(v, target);
      }
    }
    
    return unit{};
  };
}


static auto link(var from, mono to) {
  return [=](state& s) -> result<unit> {
    debug("> link:", show(from), "==", show(to));
    s.sub = s.sub.link(from, to);
    return unit{};
  };
}

static auto substitute(mono ty) {
  return [=](state& s) -> result<mono> {
    return s.sub(ty);
  };
}


template<class T>
static auto fail(std::string what) {
  return [what](state&) -> result<T> {
    return type_error(what);
  };
}

template<class M>
static auto scope(M m) {
  return [m](state& outer) {
    context scope = outer.ctx.scope();
    state inner = {scope, outer.sub};
    return m(inner);
  };
}

static auto def(symbol name, poly p) {
  return [name, p](state& s) -> result<unit> {
    s.ctx.def(name, p);
    return unit{};
  };
};

static auto generalize(mono ty) {
  return [ty](state& s) -> result<poly> {
    return s.ctx.generalize(ty);
  };
}


static auto fresh(kind k=term) {
  return [k](state& s) -> result<var> {
    return s.ctx.fresh(k);
  };
}


static auto find(symbol name) {
  return [name](state& s) -> result<poly> {
    if(auto poly = s.ctx.locals.find(name)) {
      return *poly;
    }
    
    return type_error("unbound variable: " + quote(name));
  };
};

static auto instantiate(poly p) {
  return [p](state& s) -> result<mono> {
    return s.ctx.instantiate(p);
  };
};


template<class M>
static monad<list<value<M>>> sequence(list<M> items) {
  if(!items) return pure(list<value<M>>{});
  else return items->head >>= [=](auto head) {
    return sequence(items->tail) >>= [=](auto tail) {
      return pure(head %= tail);
    };
  };
}


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

static mono ext(symbol name);

static monad<unit> unify_app_rows(app lhs, app rhs) {
  return row_match(lhs, [&](symbol lattr, mono lty, mono ltail) -> monad<unit> {
    return row_match(rhs, [&](symbol rattr, mono rty, mono rtail) -> monad<unit> {
      if(lattr == rattr) {
        return unify(lty, rty) >> unify(ltail, rtail);
      } else {
        return fresh(row) >>= [=](mono ldummy) {
          return fresh(row) >>= [=](mono rdummy) {
            return
              unify(ltail, ext(rattr)(rty)(rdummy)) >>
              unify(rtail, ext(lattr)(lty)(ldummy));
          };
        };
      }        
    });
  });
}


// note: lhs/rhs must be fully substituted
static monad<unit> unify(mono lhs, mono rhs) {
  assert(lhs.kind() == rhs.kind() && "kind error");
  
  debug("unifying:", quote(show(lhs)), "with:", quote(show(rhs)));
  
  const auto kind = lhs.kind();

  const auto lapp = lhs.cast<app>();
  const auto rapp = rhs.cast<app>();
  
  // both applications
  if(lapp && rapp) {
    if(kind == row) {
      return unify_app_rows(*lapp, *rapp);
    }

    return unify_app_terms(*lapp, *rapp);
  }    

  const auto lcst = lhs.cast<type_constant>();
  const auto rcst = rhs.cast<type_constant>();

  // both constants
  if(lcst && rcst) {
    if(*lcst == *rcst) {
      return pure(unit{});
    }
  }
  
  // variables
  return unify_vars(lhs, rhs);
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
  return unify(lhs.ctor, rhs.ctor) >>
    (substitute(lhs.arg) >>= [=](mono larg) {
      return substitute(rhs.arg) >>= [=](mono rarg) {
        return unify(larg, rarg);
      };
    });
};




////////////////////////////////////////////////////////////////////////////////
monad<mono> infer(ast::expr e);

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


static monad<mono> infer(ast::abs self) {
  return fresh() >>= [=](mono arg) {
    debug("abs:", self.arg.name, "::", show(arg));
    return scope((def(self.arg.name, poly(arg)) >> infer(self.body))
                 >>= [=](mono body) {
                   return substitute(arg >>= body) >>= [=](mono res) {
                     debug("abs res:", show(res));
                     return pure(res);
                   };
                 });
  };
  
};

static monad<mono> infer(ast::app self) {
  return infer(self.func) >>= [=](mono func) {
    return infer(self.arg) >>= [=](mono arg) {
      return fresh() >>= [=](mono ret) {
        // note: func needs to be substituted *after* arg has been inferred
        return substitute(arg >>= ret) >>= [=](mono lhs) {
          return substitute(func) >>= [=](mono rhs) {

            return unify(lhs, rhs) >> substitute(ret);
          };
        };
      };
    };
  };
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



static monad<mono> infer(ast::let self) {
  // assign fresh vars to defs
  const auto create_vars = sequence(map(self.defs, [](ast::def) {
    return fresh();
  }));

  // push let scope, create vars
  return scope(create_vars >>= [=](auto vars) {

    // populate defs scope (monomorphic)
    const auto populate_defs = sequence(zip(self.defs, vars, [](ast::def self, var a) {
      return def(self.name, mono(a));
    }));

    // infer + unify defs with vars
    const auto infer_defs = sequence(zip(self.defs, vars, [](ast::def self, var a) {
      return infer(self.value) >>= [=](mono ty) {
        return substitute(a) >>= [=](mono a) {
          // TODO detect useless definitions
          return unify(a, ty);
        };
      };
    }));

    // generalize vars + populate body scope (polymorphic)
    const auto populate_body = sequence(zip(self.defs, vars, [](ast::def self, var a) {
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


static monad<type_constant> constructor(mono ty) {
  return match(ty,
               [](type_constant self) -> monad<type_constant> { return pure(self); },
               [](app self) { return constructor(self.ctor); },
               [](var self) -> monad<type_constant> {
                 return fail<type_constant> ("type constructor is not a constant");
               });
}

static monad<mono> infer(ast::open self) {
  // attempt to extract ctor info
  return infer(self.arg) >>= [=](mono arg) {
    return constructor(arg) >>= [=](type_constant ctor) -> monad<mono> {

      if(!ctor->open) {
        // TODO use some default opening scheme instead?
        return fail<mono>("type " + quote(arg.show()) + " cannot be opened");
      }

      // instantiate open type and unify with arg
      return instantiate(ctor->open(ctor)) >>= [=](mono open) {
        return fresh() >>= [=](mono result) {
          return unify(arg >>= result, open) >> substitute(result);
        };
      };
    };
  };
}

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
static poly attr(symbol name) {
  const var a(0ul, term), rho(0ul, row);
  const mono body = record(ext(name)(a)(rho)) >>= a;
  
  return forall{a, forall{rho, body}};
};



static monad<mono> infer(ast::attr self) {
  return infer(self.arg) >>= [=](mono arg) {
    return fresh() >>= [=](mono res) {
      return instantiate(attr(self.name)) >>= [=](mono proj) {
        return (unify(proj, arg >>= res) >> substitute(res)) >>= [=](mono res) {
          debug("attr:", show(res));
          return pure(res);
        };
      };
    };
  };
};

template<class T>
static monad<mono> infer(T) {
  return fail<mono>("unimplemented: " + std::string(typeid(T).name()));
}

monad<mono> infer(ast::expr e) {
  return match(e, [=](const auto& self) {
    return infer(self);
  });
}


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
    const mono module = type_constant("module", term >>= term, false, [=](mono self) {
      const mono a = res->fresh();
      const mono b = res->fresh();
      return res->generalize(self(a) >>= record(ext("nil")(lst(a))
                                                (ext("map")((a >>= b) >>= lst(a) >>= lst(b))
                                                 (empty))));
    });

    const mono a = res->fresh();
    res->def("module", res->generalize(module(a)));
  }

  
  
  return res;
}

poly infer(std::shared_ptr<context> ctx, const ast::expr& e) {
  substitution sub;
  state s{*ctx, sub};
  
  const mono ty = match(infer(e)(s),
                        [](mono self) { return self; },
                        [](type_error error) -> mono {
                          throw error;
                        });
  // return ty;
  return ctx->generalize(ty);
}

 
} // namespace type
