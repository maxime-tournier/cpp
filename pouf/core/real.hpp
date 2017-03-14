#ifndef POUF_REAL_HPP
#define POUF_REAL_HPP

#include "traits.hpp"

using real = double;

template<>
struct traits<real> {
  
  using deriv = real;
  using scalar = real;

  static const std::size_t dim = 1;

  static scalar dot(real x, real y) { return x * y; }

  static const scalar& coord(std::size_t, const real& value) { return value; }
  static scalar& coord(std::size_t, real& value) { return value; }

  static scalar zero() { return 0; }
};


#endif
