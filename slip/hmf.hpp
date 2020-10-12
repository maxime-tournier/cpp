#ifndef HMF_HPP
#define HMF_HPP

#include "type.hpp"

namespace hmf {

using type::Type;
using type::mono;
using type::Forall;

using type::var;
using type::var_info;

using type::type_constant;
using type::type_constant_info;

using type::App;

// system F types
template<class T>
struct Sigma: variant<Type<T>, Forall<T>> {
  using Sigma::variant::variant;

  template<class Func>
  friend auto map(const Sigma& self, Func func) {
    using type = typename std::result_of<Func(T)>::type;
    using result_type = Sigma<type>;
    return match(self,
                 [&](Type<T> self) -> result_type { return map(self, func); },
                 [&](Forall<T> self) -> result_type {
                   return Forall<type>{self.arg, func(self.body)};
                 });
  }
  
};

struct sigma: fix<Sigma, sigma> {
  using sigma::fix::fix;
  
};

using rho = Type<sigma>;
using forall = Forall<sigma>;
using app = App<sigma>;

struct context;
std::shared_ptr<context> make_context();

sigma infer(std::shared_ptr<context> ctx, const ast::expr&);

}

#endif
