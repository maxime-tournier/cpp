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

  using scalar = typename traits<U>::scalar;
  using deriv = vector<N, typename traits<U>::deriv>;
  
  static const std::size_t dim = N * traits<U>::dim;

  static scalar dot(const vector<N, U>& x,
                    const vector<N, U>& y) {
    return x.dot(y);
  }

  static scalar& coord(std::size_t i, vector<N, U>& v) {
	// TODO assert this is safe
    return reinterpret_cast<scalar*>(v.data())[i];
  }

  static const scalar& coord(std::size_t i, const vector<N, U>& v) {
	// TODO assert this is safe
    return reinterpret_cast<const scalar*>(v.data())[i];
  }

  static vector<N, U> zero() {
	return vector<N, U>::Constant(traits<U>::zero());
  }
  
};



#endif
