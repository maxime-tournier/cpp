#ifndef EITHER_HPP
#define EITHER_HPP

#include "overload.hpp"

#include <type_traits>
#include <cassert>
#include <new>


// either monad
template<class Left, class Right>
class either {
  typename std::aligned_union<0, Left, Right>::type storage;
  const bool ok;
public:
  using value_type = Right;
  
  // monadic return (none/just)
  either(Right right): ok(true) {
    new (&storage) Right{std::move(right)};
  }

  either(Left left): ok(false) {
    new (&storage) Left{std::move(left)};
  }

  explicit operator bool() const { return ok; }

  template<class Cont>
  friend auto visit(const either& self, Cont cont) {
    if(self.ok) {
      return cont(*reinterpret_cast<const Right*>(&self.storage));
    } else {
      return cont(*reinterpret_cast<const Left*>(&self.storage));
    }
  }

  template<class ... Cases>
  friend auto match(const either& self, Cases... cases) {
    return visit(self, overload<Cases...>{cases...});
  }

  const Right* get() const {
    if(!ok) return nullptr;
    return reinterpret_cast<const Right*>(&storage);
  }


  Right* get() {
    if(!ok) return nullptr;
    return reinterpret_cast<Right*>(&storage);
  }
  
};


// template<class A, class F>
// static auto map(const maybe<A>& self, F f) {
//   using result_type = decltype(f(self.get()));
  
//   if(!self) {
//     return maybe<result_type>{};
//   }
  
//   return some(f(self.get()));
// }


#endif
