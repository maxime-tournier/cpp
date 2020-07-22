#ifndef MONAD_HPP
#define MONAD_HPP

#include <utility>

struct monad {
  template<class Derived, class Func, class=std::enable_if_t<std::is_base_of<monad, Derived>::value>>
  friend auto operator>>=(Derived&& self, Func func) {
    return Derived::bind(std::forward<Derived>(self), func);
  }

  template<class Derived, class Func, class=std::enable_if_t<std::is_base_of<monad, Derived>::value>>
  friend auto bind(Derived&& self, Func func) {
    return Derived::bind(std::forward<Derived>(self), func);
  }
  
};


#endif
