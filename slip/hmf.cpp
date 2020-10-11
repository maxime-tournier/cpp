#include "hmf.hpp"

#include "hamt.hpp"
#include "either.hpp"
#include "ast.hpp"
#include "common.hpp"

namespace hmf {

struct context {
  std::size_t depth = 0;
  hamt::map<symbol, sigma> locals;
};


struct state {
  context ctx;
};

template<class T>
struct success: T, state {
  using value_type = T;
  success(T value, state s): T(value), state(s) { }
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


////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
static monad<sigma> infer(ast::var self) {
  return [=](state s) -> result<sigma> {
    if(auto res = s.ctx.locals.find(self.name)) {
      return make_success(*res, s);
    }

    return type_error("unbound variable: " + quote(self.name));    
  };
}




} // namespace hmf
