#ifndef TYPES_HPP
#define TYPES_HPP

#include <Eigen/Core>
#include <Eigen/Sparse>

using real = double;

template<int N = Eigen::Dynamic, class U = real>
using vector = Eigen::Matrix<U, N, 1>;

using vec1 = vector<1>;
using vec2 = vector<2>;
using vec3 = vector<3>;
using vec4 = vector<4>;
using vec6 = vector<6>;

template<class G> struct dofs;

template<class G> struct traits;

template<class G>
using deriv = typename traits<G>::deriv;

template<class G>
using scalar = typename traits<G>::scalar;


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
    return reinterpret_cast<scalar*>(v.data())[i];
  }

  static const scalar& coord(std::size_t i, const vector<N, U>& v) {
    return reinterpret_cast<scalar*>(v.data())[i];
  }
  
};



template<>
struct traits<real> {
  
  using deriv = real;
  using scalar = real;

  static const std::size_t dim = 1;

  static scalar dot(real x, real y) { return x * y; }

  static const scalar& coord(std::size_t, const real& value) { return value; }
  static scalar& coord(std::size_t, real& value) { return value; }  
};

using rmat = Eigen::SparseMatrix<real, Eigen::RowMajor>;
using cmat = Eigen::SparseMatrix<real, Eigen::ColMajor>;

using triplet = Eigen::Triplet<real>;
using triplet_iterator = std::back_insert_iterator< std::vector<triplet> >;



#endif
