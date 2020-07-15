#ifndef FIX_HPP
#define FIX_HPP

// recursion schemes ftw!11
template<template<class> class F, class Derived>
struct fix: F<Derived> {
  using base = F<Derived>;
  using base::base;

  template<class Alg, class A, class B>
  static constexpr A result(A (Alg::*)(B) const);
  
  template<class Alg>
  friend auto cata(const fix& self, const Alg& alg) -> decltype(result(&Alg::operator())) {
    return alg(map(self, [&](auto x) { return cata(x, alg); }));
  }
};


#endif
