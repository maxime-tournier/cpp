#ifndef MATH_HPP
#define MATH_HPP

#include <Eigen/Core>
#include <Eigen/Geometry>

template<class T, int M = Eigen::Dynamic, int N = Eigen::Dynamic>
using matrix = Eigen::Matrix<T, M, N>;

template<class T, int M = Eigen::Dynamic>
using vector = matrix<T, M, 1>;

using real = double;

using vec = vector<real>;

using vec4 = vector<real, 4>;
using vec3 = vector<real, 3>;
using vec2 = vector<real, 2>;
using vec1 = vector<real, 1>;

using mat = matrix<real>;
using mat4x4 = matrix<real, 4, 4>;
using mat3x3 = matrix<real, 3, 3>;


template<class T>
using quaternion = Eigen::Quaternion<T>;

using quat = quaternion<real>;


struct rigid {
  quat orient;
  vec3 pos;

  rigid(): orient(1, 0, 0, 0), pos(0, 0, 0) {}

  static rigid translation(real x, real y, real z) {
    rigid res;
    res.pos = {x, y, z};
    return res;
  }

  static rigid translation(vec3 t) {
    return translation(t.x(), t.y(), t.z());
  }

  static rigid rotation(quat q) {
    rigid res;
    res.orient = q;
    return res;
  }
  
  rigid operator*(const rigid& other) const {
    rigid res;
    res.orient = orient * other.orient;
    res.pos = pos + orient * other.pos;
    return res;
  }

  rigid inv() const {
    rigid res;
    res.orient = orient.conjugate();
    res.pos = -(res.orient * pos);
    return res;
  }
  
};





#endif
