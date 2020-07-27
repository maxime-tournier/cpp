#include "type.hpp"
#include "ast.hpp"
#include "either.hpp"
#include "unit.hpp"

#include <functional>
#include <sstream>

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


static std::string varid(var self) {
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
                     return "<var:" + varid(self) + ">";
                   }

                   return it->second;
                 },
                 [](App<result> self) -> result {
                   if(self.ctor.flip) {
                     return self.arg.value + " " + self.ctor.value;
                   } else {
                     return self.ctor.value + " " + self.arg.value;
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

static std::string varname(std::size_t index, kind k) {
  std::string res;
  res += 'a' + index;

  if(k == row) {
    res += "...";
  }
  
  return res;
}

static repr_type label(list<var> vars, repr_type repr={}) {
  for(auto var: vars) {
    repr.emplace(var, varname(repr.size(), var->kind));
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

template<class T>
static std::string quote(const T& self) {
  std::stringstream ss;
  ss << '"' << self << '"';
  return ss.str();
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
    return {table.set(a.get(), std::move(ty))};
  }
  
  mono operator()(mono ty) const {
    return cata(ty, [&](Mono<mono> self) {
      return match(self,
                   [&](var self) -> mono {
                     if(auto res = table.find(self.get())) {
                       return *res;
                     }
                     return self;
                   },
                   [](auto self) -> mono { return self; });                   
    });
  }

};


////////////////////////////////////////////////////////////////////////////////
// context

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

template<class T>
using result = either<type_error, T>;

template<class T>
using monad = std::function<result<T>(context& ctx, substitution& sub)>;

template<class M>
using value = typename std::result_of<M(context& ctx,
                                        substitution&)>::type::value_type;

template<class T>
static auto pure(T value) {
  return [value](context& ctx, substitution&) -> result<T> { return value; };
}

template<class MA, class Func, class A=value<MA>>
static auto bind(MA self, Func func) {
  return [self, func](context& ctx, substitution& sub) {
    return self(ctx, sub) >>= [&](const A& value) {
      return func(value)(ctx, sub);
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

static auto upgrade(mono ty, std::size_t max) {
  return [=](context& ctx, substitution& sub) -> result<unit> {
    for(auto v: ty.vars()) {
      if(v->depth > max) {
        sub = sub.link(v, var(max, v->kind));
      }
    }
    
    return unit{};
  };
}


static auto link(var from, mono to) {
  return [=](context& ctx, substitution& sub) -> result<unit> {
    sub = sub.link(from, to);
    return unit{};
  };
}

static auto substitute(mono ty) {
  return [=](context& ctx, substitution& sub) -> result<mono> {
    return sub(ty);
  };
}


template<class T>
static auto fail(std::string what) {
  return [what](context& ctx, substitution&) -> result<T> {
    return type_error(what);
  };
}

template<class M>
static auto scope(M m) {
  return [m](context& ctx, substitution& sub) {
    auto scope = ctx.scope();
    return m(scope, sub);
  };
}

static auto def(symbol name, poly p) {
  return [name, p](context& ctx, substitution& sub) -> result<unit> {
    ctx.def(name, p);
    return unit{};
  };
};


static auto fresh(kind k=term) {
  return [k](context& ctx, substitution& sub) -> result<var> {
    return ctx.fresh(k);
  };
}


static auto find(symbol name) {
  return [name](context& ctx, substitution& sub) -> result<poly> {
    if(auto poly = ctx.locals.find(name)) {
      return *poly;
    }
    
    return type_error("unbound variable: " + quote(name));
  };
};

static auto instantiate(poly p) {
  return [p](context& ctx, substitution& sub) -> result<mono> {
    return ctx.instantiate(p);
  };
};

////////////////////////////////////////////////////////////////////////////////
// unification
////////////////////////////////////////////////////////////////////////////////
static monad<unit> unify(mono lhs, mono rhs);

static monad<unit> unify_terms(mono lhs, mono rhs);
static monad<unit> unify_rows(mono lhs, mono rhs);
static monad<unit> unify_vars(mono lhs, mono rhs);

static monad<unit> unify_rows(mono lhs, mono rhs) {
    return fail<unit>("unimplemented: row unification");
}


// note: lhs/rhs must be fully substituted
static monad<unit> unify(mono lhs, mono rhs) {
  assert(lhs.kind() == rhs.kind());

  const auto kind = lhs.kind();
  if(kind == row) {
    return unify_rows(lhs, rhs);
  }

  return unify_terms(lhs, rhs);
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
    return link(*lvar, rhs) >> upgrade(rhs, (*lvar)->depth);
  } else if(rvar) {
    return link(*rvar, lhs) >> upgrade(lhs, (*rvar)->depth);    
  }
  
  return fail<unit>("cannot unify types " + quote(lhs.show()) +
                    " and " + quote(rhs.show()));  
}


static monad<unit> unify_terms(mono lhs, mono rhs) {
  const auto lapp = lhs.cast<app>();
  const auto rapp = rhs.cast<app>();
  
  // both applications
  if(lapp && rapp) {
    return unify(lapp->ctor, rapp->ctor) >>
      (substitute(lhs.get<app>().arg) >>= [=](mono larg) {
        return substitute(rhs.get<app>().arg) >>= [=](mono rarg) {
          return unify(larg, rarg);
        };
      });
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
    return instantiate(p);
  };
};


static monad<mono> infer(ast::abs self) {
  return fresh() >>= [=](mono arg) {
    return scope((def(self.arg.name, poly(arg)) >> infer(self.body))
                 >>= [=](mono body) {
                   return substitute(arg >>= body);
                 });
  };
  
};


static const bool debug = std::getenv("SLIP_DEBUG");

static auto print(mono lhs, mono rhs) {
  static repr_type repr;
            
  repr = label(lhs.vars(), std::move(repr));
  repr = label(rhs.vars(), std::move(repr));
        
  std::clog << "unifying: " << lhs.show(repr)
            << " with: " << rhs.show(repr)
            << std::endl;

}

static monad<mono> infer(ast::app self) {
  return infer(self.func) >>= [=](mono func) {
    return infer(self.arg) >>= [=](mono arg) {
      return fresh() >>= [=](mono ret) {
        // note: func needs to be substituted *after* arg has been inferred
        return substitute(arg >>= ret) >>= [=](mono lhs) {
          return substitute(func) >>= [=](mono rhs) {

            if(debug) {
              print(lhs, rhs);
            }
      
            return unify(lhs, rhs) >> substitute(ret);
          };
        };
      };
    };
  };
};


// row extension type constructor
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
        std::clog << proj.show() << std::endl;
        std::clog << (arg >>= res).show() << std::endl;        
        
        return unify(proj, arg >>= res) >> substitute(res);
      };
    };
  };
};

template<class T>
static monad<mono> infer(T) {
  return fail<mono>("unimplemented: " + std::string(typeid(T).name()));
}

monad<mono> infer(ast::expr e) {
  return match(e, [=](const auto& self) { return infer(self); });
}


// user-facing api
std::shared_ptr<context> make_context() {
  return std::make_shared<context>();
}

poly infer(std::shared_ptr<context> ctx, const ast::expr& e) {
  substitution sub;
  const mono ty = match(infer(e)(*ctx, sub),
                        [](mono self) { return self; },
                        [](type_error error) -> mono {
                          throw error;
                        });
  // return ty;
  return ctx->generalize(ty);
}

 
} // namespace type

