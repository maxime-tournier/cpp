#ifndef FUNCTOR_HPP
#define FUNCTOR_HPP

#include <utility>

struct functor {
  template<class Derived, class Func, class=std::enable_if_t<std::is_base_of<functor, Derived>::value>>
  friend auto operator|=(Derived&& self, Func func) {
    return Derived::map(std::forward<Derived>(self), func);
  }

  template<class Derived, class Func, class=std::enable_if_t<std::is_base_of<functor, Derived>::value>>
  friend auto map(Derived&& self, Func func) {
    return Derived::map(std::forward<Derived>(self), func);
  }
  
};


#endif
