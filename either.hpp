#ifndef EITHER_HPP
#define EITHER_HPP

#include "functor.hpp"
#include "monad.hpp"

#include "overload.hpp"

#include <type_traits>
#include <cassert>
#include <new>


// either monad
template<class Left, class Right>
class either: public functor,
              public monad {
  typename std::aligned_union<0, Left, Right>::type storage;
  const bool ok;

  template<class T>
  T& cast() {
    return *reinterpret_cast<T*>(&storage);
  }

  template<class T>
  const T& cast() const {
    return *reinterpret_cast<const T*>(&storage);
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
  static auto visit(Self&& self, Cont cont) {
    if(self.ok) {
      return cont(std::forward<Self>(self).right());
    } else {
      return cont(std::forward<Self>(self).left());
    }
  }

  template<class Cont>
  friend auto visit(either&& self, Cont cont) {
    return either::visit(std::move(self), std::move(cont));
  }

  template<class Cont>
  friend auto visit(const either& self, Cont cont) {
    return either::visit(self, std::move(cont));
  }
  
  
  template<class Self, class ... Cases>
  static auto match(Self&& self, Cases... cases) {
    return visit(std::forward<Self>(self), overload<Cases...>{cases...});
  }

  template<class ... Cases>
  friend auto match(either&& self, Cases&&... cases) {
    return match(std::move(self), std::forward<Cases>(cases)...);
  }

  template<class ... Cases>
  friend auto match(const either& self, Cases&&... cases) {
    return match(self, std::forward<Cases>(cases)...);
  }
  
  
  const Right* get() const {
    if(!ok) return nullptr;
    return reinterpret_cast<const Right*>(&storage);
  }


  Right* get() {
    if(!ok) return nullptr;
    return reinterpret_cast<Right*>(&storage);
  }

  const Right& right() const {
    assert(ok);
    return cast<Right>();
  };
  
  Right& right() {
    assert(ok);
    return cast<Right>();
  };

  const Left& left() const {
    assert(!ok);
    return cast<Left>();
  };
  
  Left& left() {
    assert(!ok);
    return cast<Left>();
  };

  
  template<class Func>
  static auto map(either&& self, Func func) {
    using value_type = typename std::result_of<Func(Right)>::type;
    using result_type = either<Left, value_type>;
    
    return match(self,
                 [](Left& left) -> result_type {
                   return std::move(left);
                 },
                 [&](Right& right) -> result_type {
                   return func(std::move(right));
                 });
  }

  template<class Func>
  static auto map(const either& self, Func func) {
    using value_type = typename std::result_of<Func(Right)>::type;
    using result_type = either<Left, value_type>;
    
    return match(self,
                 [](const Left& left) -> result_type {
                   return left;
                 },
                 [&](const Right& right) -> result_type {
                   return func(right);
                 });
  }
  

  template<class Func>
  static auto bind(either&& self, const Func& func) {
    using result_type = typename std::result_of<Func(Right)>::type;
    
    return match(self,
                 [](Left& left) -> result_type {
                   return std::move(left);
                 },
                 [&](Right& right) -> result_type {
                   return func(std::move(right));
                 });
  }

  template<class Func>
  static auto bind(const either& self, const Func& func) {
    using result_type = typename std::result_of<Func(Right)>::type;
    
    return match(self,
                 [](Left& left) -> result_type {
                   return std::move(left);
                 },
                 [&](Right& right) -> result_type {
                   return func(std::move(right));
                 });
  }
  

};



#endif
