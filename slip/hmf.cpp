#include "hmf.hpp"

#include "hamt.hpp"
#include "unit.hpp"
#include "either.hpp"
#include "ast.hpp"
#include "common.hpp"

#include <algorithm>

namespace hmf {

const rho func = type_constant("->", term >>= term >>= term, true);

namespace impl {

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


struct show_info {
  hamt::map<const var_info*, std::string> table;
  std::size_t count = 0;

  show_info add(const var_info* v) const {
    return {table.set(v, var_name(count, v->kind)), count + 1};
  }

  const std::string* find(const var_info* v) const { return table.find(v); }
};

static std::string var_id(var self) {
  std::stringstream ss;
  ss << self.get();
  std::string s = ss.str();
  
  return s.substr(s.size() - 6);
};


static std::string show(sigma self, show_info info = {});

static std::string show(rho self, show_info info) {
  return match(self,
               [](type_constant self) -> std::string { return self->name.repr; },
               [=](var self) {
                 if(auto repr = info.find(self.get())) {
                   return *repr;
                 }

                 return var_id(self);
               },
               [=](app self) {
                 // TODO parens, nested foralls, etc
                 return show(self.ctor, info) + " " + show(self.arg, info);
               });
}

static std::string show(sigma self, show_info info) {
  return match(self,
               [=](forall self) { return show(self.body, info.add(self.arg.get())); },
               [=](rho self) { return show(self, info); });
}

} // namespace impl


std::string sigma::show() const {
  return impl::show(*this);
}



////////////////////////////////////////////////////////////////////////////////

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


static auto skolem(kind k = term) {
  return [=](state s) -> result<var> {
    type::flags_type flags;
    flags.set(type::skolem);
    
    return make_success(var(s.ctx.depth, k, flags), s);
  };
}



namespace impl {

static sigma substitute(substitution sub, sigma self);

static sigma substitute(substitution sub, rho self) {
  return match(self,
               [](type_constant self) -> sigma { return rho(self); },
               [&](var self) -> sigma {
                 if(auto res = sub.find(self.get())) {
                   // TODO can we ever unify a var and a sigma type? apparently
                   // yes
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
    const sigma res =
    foldr(free_vars(self), sigma(self), [=](var v, sigma body) -> sigma {
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


namespace impl {

template<class Func>
static monad<substitution> instantiate(sigma self, Func func) {
  return match(self,
               [=](forall self) -> monad<substitution> {
                 return func(self.arg->kind) >>= [=](rho v) {
                   return instantiate(self.body, func) |= [=](substitution sub) {
                     return sub.set(self.arg.get(), v);
                   };
                 };
               },
               [=](rho self) -> monad<substitution>  {
                 return pure(substitution());
               });
};


template<class Func>
static monad<rho> instantiate_substitute(sigma self, Func func) {
  return impl::instantiate(self, func) |= [=](substitution sub) {
    return match(impl::substitute(sub, peel(self)),
                 [](forall) -> rho { throw std::logic_error("derp"); },
                 [](rho self) { return self; });
  };
}

}


static monad<rho> instantiate(sigma self) {
  return impl::instantiate_substitute(self, fresh);
};

static monad<rho> skolemize(sigma self) { 
  return impl::instantiate_substitute(self, skolem);  
};

static monad<rho> instantiate_subsume(sigma self) { 
  // TODO we could store instantiated variables in state here
  
};


static monad<unit> unify(sigma lhs, sigma rhs) {
  
}


static monad<sigma> infer(ast::abs self) {
  // TODO handle typed args
  const auto defs = sequence(map(self.args, [=](ast::arg arg) {
    return fresh() >>= [=](var self) {
      return def(arg.name(), rho(self)) >> pure(self);
    };
  }));

  return scope(defs >>= [=](list<var> defs) {
    // TODO optimize generalization/instantiation
    return (infer(self.body) >>= instantiate) >>= [=](rho body) {
      const monad<rho> init = pure(body);
      return foldr(defs, init, [](rho arg, monad<rho> res) -> monad<rho> {
        return substitute(arg) >>= [=](sigma arg) {
          // TODO check arg is monomorphic
          return res |= [=](rho res) -> rho {
            return app{rho{app{func, arg}}, res};
          };
        };
      }) |= [](rho res) -> sigma { return res; };
    };
  });
};




static monad<unit> subsume(sigma expected, sigma offered) {
// TODO assert both are in normal form  
  return skolemize(expected) >>= [=](rho expected) {
    return instantiate_subsume(offered) >>= [=](rho offered) {
      // TODO cleanup substitution from instantiated vars in offered
      return unify(expected, offered);
    };
  };
};

// static monad<sigma> infer(ast::app self) {
//   return infer(self.func) >>= [=](sigma func) {
//     // TODO funmatch on peel(func)
//     return funmatch(peel(func), [=](sigma expected, sigma ret) {
//       // TODO multiple args
//       return infer(self.args->head) >>= [=](sigma offered) {
//         return subsume(expected, offered) >>= [=](substitution poly) {
//           return substitute(ret) >>= [=](sigma ret) {
//             return poly(ret);
//           };
//         };
//       };
//     });
//   };
// }


static monad<sigma> infer(ast::expr self) {
  return match(self, [](auto self) { return infer(self); });
}

std::shared_ptr<context> make_context() {
  return std::make_shared<context>();
}


sigma infer(std::shared_ptr<context> ctx, const ast::expr& self) {
  state s{*ctx, {}};
  return match(infer(self)(s),
               [](auto result) { return result.value; },
               [](type_error e) -> sigma { throw e; });
}

} // namespace hmf
