#include "hmf.hpp"

#include "hamt.hpp"
#include "unit.hpp"
#include "either.hpp"
#include "ast.hpp"
#include "common.hpp"

namespace hmf {

const rho func = type_constant("->", term >>= term >>= term, true);

struct context {
  std::size_t depth = 0;
  hamt::map<symbol, sigma> locals;
};

using substitution = hamt::map<const var_info*, sigma>;


struct state {
  context ctx;
  substitution sub;
};

template<class T>
struct success {
  using value_type = T;
  T value;
  
  struct state state;
};

template<class T>
static success<T> make_success(T value, state s) { return {value, s}; }

struct type_error: std::runtime_error {
  type_error(std::string what): std::runtime_error("type error: " + what) {}
};

template<class T>
using result = either<type_error, success<T>>;

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

template<class M, class T = value<M>>
static monad<list<T>> sequence(list<M> ms) {
  monad<list<T>> init = pure(list<T>{});
  return foldr(ms, init, [](M m, monad<list<T>> tail) -> monad<list<T>> {
    return m >>= [=](T head) {
      return tail >>= [=](list<T> tail) {
        return pure(head %= tail);
      };
    };
  });
}


////////////////////////////////////////////////////////////////////////////////
static const auto find = [](symbol name) {
  return [=](state s) -> result<sigma> {
    if(auto res = s.ctx.locals.find(name)) {
      return make_success(*res, s);
    }

    return type_error("unbound variable: " + quote(name));    
  };
};


static auto fresh(kind k = term) {
  return [=](state s) -> result<var> {
    return make_success(var(s.ctx.depth, k), s);
  };
}



namespace impl {

static sigma substitute(substitution sub, sigma self);

static sigma substitute(substitution sub, rho self) {
  return match(self,
               [](type_constant self) -> sigma { return rho(self); },
               [&](var self) -> sigma {
                 if(auto res = sub.find(self.get())) {
                   return substitute(sub, *res);
                 }

                 return rho(self);
               },
               [&](app self) -> sigma {
                 return rho(app{substitute(sub, self.ctor),
                                substitute(sub, self.arg)});
               });
}

static sigma substitute(substitution sub, sigma self) {
  return match(self,
               [=](forall self) -> sigma {
                 assert(!sub.find(self.arg.get()));
                 return forall{self.arg, substitute(sub, self.body)};
               },
               [=](rho self) -> sigma {
                 return substitute(sub, self);
               });
};

} // namespace impl


static auto substitute(sigma self) {
  return [=](state s) -> result<sigma> {
    return make_success(impl::substitute(s.sub, self), s);
  };
};


static auto def(symbol name, sigma value) {
  return [=](state s) -> result<unit> {
    return make_success(
        unit{},
        state{context{s.ctx.depth, s.ctx.locals.set(name, sigma(value))},
              s.sub});
  };
}


static const auto scope = [](auto m) {
  return [m](state s) {
    context ctx = {s.ctx.depth + 1,
                   s.ctx.locals};
    state inner = {ctx, s.sub};

    // update substitution from scope, drop context    
    return m(inner) |= [=](auto self) {
      return make_success(self.value, state{s.ctx, self.state.sub});
    };
  };
};

using set = hamt::map<const var_info*, unit>;
static list<var> free_vars(sigma self, set bound={});

static list<var> free_vars(rho self, set bound) {
  return match(self,
               [=](type_constant self) { return list<var>{}; },
               [=](var self) {
                 if(bound.find(self.get())) {
                   return list<var>{};
                 }

                 return self %= list<var>{};
               },
               [=](app self) {
                 return concat(free_vars(self.ctor, bound),
                               free_vars(self.arg, bound));
               });
}


static list<var> free_vars(sigma self, set bound) {
  return match(self,
               [=](forall self) {
                 return free_vars(self.body, bound.set(self.arg.get(), {}));
               },
               [=](rho self) {
                 return free_vars(self, bound);
               });
}


// note: self must be fully substituted
static auto generalize(rho self) {

  // TODO instantiate/substitute variables ?
  return [=](state s) -> result<sigma> {
    const sigma res = foldr(free_vars(self), sigma(self), [=](var v, sigma body) -> sigma {
      if(v->depth < s.ctx.depth) {
        // var is bound somewhere in enclosing scope: skip
        return body;
      } else {
        return forall{v, body};
      }
    });
    
    return make_success(res, s);
  };
}


////////////////////////////////////////////////////////////////////////////////
static monad<sigma> infer(ast::expr self);

template<class T>
static monad<sigma> infer(T self) {
  return [=](state) {
    return type_error("unimplemented: " + std::string(typeid(T).name()));
  };
}

static monad<sigma> infer(ast::var self) {
  return find(self.name) >>= substitute;
}


static monad<sigma> infer(ast::let self) {
  // TODO handle (mutually) recursive defs
  const auto defs = sequence(map(self.defs, [](auto self) {
    return infer(self.value) >>= [=](sigma value) {
      return def(self.name, value);
    };
  }));

  return scope(defs >> infer(self.body));
}

static rho peel(sigma self) {
  return match(self,
               [](forall self) { return peel(self.body); },
               [](rho self) { return self; });
}


static monad<sigma> infer(ast::abs self) {
  // TODO handle typed args
  const auto defs = sequence(map(self.args, [=](ast::arg arg) {
    return fresh() >>= [=](var self) {
      return def(arg.name(), rho(self)) >> pure(self);
    };
  }));

  return scope(defs >>= [=](list<var> defs) {
    return infer(self.body) >>= [=](sigma body) {
      return generalize(foldr(defs, peel(body), [](rho arg, rho result) -> rho {
        return app{rho{app{func, arg}}, result};
      }));
    };
  });
};

static monad<sigma> infer(ast::expr self) {
  return match(self, [](auto self) { return infer(self); });
}



} // namespace hmf
