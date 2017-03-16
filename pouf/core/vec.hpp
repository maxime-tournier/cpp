#ifndef POUF_VEC_HPP
#define POUF_VEC_HPP

#include <Eigen/Core>

#include "real.hpp"

template<int N = Eigen::Dynamic, class U = real>
using vector = Eigen::Matrix<U, N, 1>;

using vec1 = vector<1>;
using vec2 = vector<2>;
using vec3 = vector<3>;
using vec4 = vector<4>;
using vec6 = vector<6>;
using vec = vector<>;


template<int N, class U>
struct traits< vector<N, U> > {

  using scalar_type = scalar<U>;
  using deriv_type = vector<N, deriv<U> >;
  
  static const std::size_t dim = N * traits<U>::dim;

  static scalar_type dot(const vector<N, U>& x,
                         const vector<N, U>& y) {
    return x.dot(y);
  }

  static scalar_type& coord(std::size_t i, vector<N, U>& v) {
	// TODO assert this is safe
    return reinterpret_cast<scalar_type*>(v.data())[i];
  }

  static const scalar_type& coord(std::size_t i, const vector<N, U>& v) {
	// TODO assert this is safe
    return reinterpret_cast<const scalar_type*>(v.data())[i];
  }

  static vector<N, U> zero() {
	return vector<N, U>::Constant(traits<U>::zero());
  }


  // additive lie group structure
  // TODO multiplicative as well?
  using group_type = vector<N, U>;
  
  static group_type id() { return zero(); }
  static group_type inv(const group_type& x) { return -x; }
  static group_type prod(const group_type& x, const group_type& y) { return x + y; }

  static group_type exp(const deriv_type& x) { return x; }
  

  
};



#endif
