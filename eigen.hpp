#ifndef EIGEN_HPP
#define EIGEN_HPP

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace eigen {
template<class T, int M = Eigen::Dynamic, int N = Eigen::Dynamic>
using matrix = Eigen::Matrix<T, M, N>;

template<class T, int M = Eigen::Dynamic>
using vector = matrix<T, M, 1>;

using real = double;

using vec = vector<real>;
using vec3 = vector<real, 3>;
using vec2 = vector<real, 2>;
using vec1 = vector<real, 1>;

using mat = matrix<real>;
using mat4x4 = matrix<real, 4, 4>;
using mat3x3 = matrix<real, 3, 3>;


template<class T>
using quaternion = Eigen::Quaternion<T>;

using quat = quaternion<real>;
  
} // namespace eigen

#endif
