#ifndef POUF_REAL_HPP
#define POUF_REAL_HPP

#include "traits.hpp"

using real = double;

template<>
struct traits<real> {
  
  using deriv_type = real;
  using scalar_type = real;

  static const std::size_t dim = 1;

  static scalar_type dot(real x, real y) { return x * y; }

  static const scalar_type& coord(std::size_t, const real& value) { return value; }
  static scalar_type& coord(std::size_t, real& value) { return value; }

  static scalar_type zero() { return 0; }


  // additive lie group structure
  using group_type = real;
  
  static group_type id() { return zero(); }
  static group_type inv(const group_type& x) { return -x; }
  static group_type prod(const group_type& x, const group_type& y) { return x + y; }

  static group_type exp(const deriv_type& x) { return x; }

  static const char* name() { return "real"; }

  static deriv_type AdT(const group_type&, const deriv_type& x) { return x; }
  static deriv_type Ad(const group_type&, const deriv_type& x) { return x; }  
};


#endif
