#include "type.hpp"
#include "ast.hpp"
#include "either.hpp"
#include "unit.hpp"

#include <functional>

////////////////////////////////////////////////////////////////////////////////
namespace type {

const kind term = kind_constant::make("*");
const kind row = kind_constant::make("@");

const mono func = type_constant::make("->", term >>= term >>= term, true);

const mono boolean = type_constant::make("bool", term);
const mono integer = type_constant::make("int", term);
const mono number = type_constant::make("num", term);
const mono string = type_constant::make("str", term);

struct type_error: std::runtime_error {
  type_error(std::string what): std::runtime_error("type error: " + what) { }
};

////////////////////////////////////////////////////////////////////////////////
// kinds
////////////////////////////////////////////////////////////////////////////////
ref<kind_constant> kind_constant::make(symbol name) {
  return std::make_shared<const kind_constant>(name);
}

ref<type_constant> type_constant::make(symbol name, struct kind kind, bool flip) {
  return std::make_shared<const type_constant>(name, kind, flip);
}

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
                 [](auto) { return true; });
}

std::string kind::show() const {
  return match(*this,
               [&](ref<kind_constant> self) -> std::string {
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
                 [](ref<type_constant> self) -> result {
                   return {self->name.repr, self->flip};
                 },
                 [&](ref<var> self) -> result {
                   const auto it = repr.find(self.get());
                   if(it == repr.end()) {
                     return std::string("<var>");
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
                 [](ref<type_constant> self) { return self->kind; },
                 [](ref<var> self) { return self->kind; },
                 [](App<struct kind> self) {
                   return self.ctor.get<ctor>().to;
                 });
  });
}

list<ref<var>> mono::vars() const {
  return cata(*this, [](Mono<list<ref<var>>> self) {
    return match(self,
                 [](ref<var> self) { return self %= list<ref<var>>{}; },
                 [](App<list<ref<var>>> self) {
                   return concat(self.ctor, self.arg);
                 },
                 [](ref<type_constant> self) { return list<ref<var>>{}; });
  });
}


////////////////////////////////////////////////////////////////////////////////
mono poly::body() const {
  return cata(*this, [](Poly<mono> self) {
    return match(self,
                 [](Forall<mono> self) { return self.body; },
                 [](mono self) { return self; });
  });
}



list<ref<var>> poly::bound() const {
  return cata(*this, [](Poly<list<ref<var>>> self) {
    return match(self,
                 [](Forall<list<ref<var>>> self) {
                   return self.arg %= self.body;
                 }, 
                 [](mono self) { return list<ref<var>>{}; });
  });
}

static std::string varname(std::size_t index) {
  std::string res;
  res += 'a' + index;
  return res;
}

static repr_type label(list<ref<var>> vars, repr_type repr={}) {
  for(auto var: vars) {
    repr.emplace(var.get(), varname(repr.size()));
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
  using table_type = hamt::map<const var*, mono>;
  table_type table;

  substitution(table_type table): table(table) { }
public:
  substitution() = default;
  
  substitution link(const var* a, mono ty) const {
    return {table.set(a, std::move(ty))};
  }
  
  mono operator()(mono ty) const {
    return cata(ty, [&](Mono<mono> self) {
      return match(self,
                   [&](ref<var> self) -> mono {
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

  ref<var> fresh(struct kind kind=term) const {
    return std::make_shared<var>(depth, kind);
  }
  
  mono instantiate(poly p, struct kind kind=term) const {
    const substitution sub =
        foldr(p.bound(), substitution{}, [&](ref<var> a, substitution sub) {
          return sub.link(a.get(), fresh(kind));
        });

    return sub(p.body());
  };


  // quantify unbound variables
  list<ref<var>> quantify(list<ref<var>> vars) const {
    return foldr(vars, list<ref<var>>{}, [&](ref<var> a, list<ref<var>> rest) {
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
    
    const auto freshes = map(foralls, [&](ref<var> a) {
      const auto res = fresh(a->kind);
      sub = sub.link(a.get(), res);
      return res;
    });

    return foldr(freshes, poly(sub(ty)), [](ref<var> a, poly p) -> poly {
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
using value = typename std::result_of<M(context& ctx, substitution&)>::type::value_type;

template<class T>
static auto pure(T value) { return [value](context& ctx, substitution&) -> result<T> { return value; }; }

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


static auto link(const var* from, mono to) {
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
  return [k](context& ctx, substitution& sub) -> result<ref<var>> {
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

// note: lhs/rhs must be fully substituted
static monad<unit> unify(mono lhs, mono rhs) {
  assert(lhs.kind() == rhs.kind());

  const auto lapp = lhs.cast<app>();
  const auto rapp = rhs.cast<app>();
  
  // both applications
  if(lapp && rapp) {
    return unify(lapp->ctor, rapp->ctor) >>
      (substitute(lapp->arg) >>= [=](mono larg) {
        return substitute(rapp->arg) >>= [=](mono rarg) {
          return unify(larg, rarg);
        };
      });
  }

  const auto lvar = lhs.cast<ref<var>>();
  const auto rvar = rhs.cast<ref<var>>();  
  
  // both variables
  if(lvar && rvar) {
    const auto target = std::make_shared<const var>(std::min((*lvar)->depth,
                                                             (*rvar)->depth),
                                                    (*lvar)->kind);
    // TODO create target *inside* monad?
    return link(lvar->get(), target) >> link(rvar->get(), target);
  } else if(lvar) {
    // TODO upgrade?    
    return link(lvar->get(), rhs);
  } else if(rvar) {
    // TODO upgrade?
    return link(rvar->get(), lhs);    
  }
  
  const auto lcst = lhs.cast<ref<type_constant>>();
  const auto rcst = rhs.cast<ref<type_constant>>();

  // both constants
  if(lcst && rcst) {
    if(*lcst == *rcst) {
      return pure(unit{});
    }
  }

  return fail<unit>("cannot unify types " + quote(lhs.show()) +
                    " and " + quote(rhs.show()));
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
    return scope((def(self.arg.name, poly(arg)) >> infer(self.body)) >>= [=](mono body) {
      return substitute(arg >>= body);
    });
  };
  
};


static const bool debug = false;

static monad<mono> infer(ast::app self) {
  return infer(self.func) >>= [=](mono func) {
    return infer(self.arg) >>= [=](mono arg) {
      return fresh() >>= [=](mono ret) {
      // note: func needs to be substituted *after* arg has been inferred

        return substitute(arg >>= ret) >>= [=](mono lhs) {
          return substitute(func) >>= [=](mono rhs) {
            if(debug) {
              static repr_type repr;
            
              repr = label(lhs.vars(), std::move(repr));
              repr = label(rhs.vars(), std::move(repr));
        
              std::clog << "unifying: " << lhs.show(repr)
                        << " with: " << rhs.show(repr)
                        << std::endl;
            }
      
            return unify(lhs, rhs) >> substitute(ret);
          };
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

