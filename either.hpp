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

  template<class T, class Derived>
  friend T& cast(Derived* self) {
    return *reinterpret_cast<T*>(&self->storage);
  }
  
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

  template<class Self, class Cont>
  friend auto visit(Self&& self, Cont cont) {
    if(self.ok) {
      return cont(std::forward<Self>(self).right());
    } else {
      return cont(std::forward<Self>(self).left());
    }
  }

  template<class Self, class ... Cases>
  friend auto match(Self&& self, Cases... cases) {
    return visit(std::forward<Self>(self), overload<Cases...>{cases...});
  }
  
  const Right* get() const {
    if(!ok) return nullptr;
    return reinterpret_cast<const Right*>(&storage);
  }


  Right* get() {
    if(!ok) return nullptr;
    return reinterpret_cast<Right*>(&storage);
  }

  const Right& right() const& {
    assert(ok);
    return cast<const Right>(this);
  };
  
  Right& right() {
    assert(ok);
    return cast<Right>(this);
  };

  const Left& left() const {
    assert(!ok);
    return cast<const Left>(this);
  };
  
  Left& left() {
    assert(!ok);
    return cast<Left>(this);
  };
  
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
