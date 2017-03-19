#ifndef CORE_QUAT_HPP
#define CORE_QUAT_HPP

#include <core/real.hpp>
#include <Eigen/Geometry>

// a unit quaternion
template<class U>
class quaternion : public Eigen::Quaternion<U, Eigen::DontAlign> {
  using base = typename quaternion::Quaternion;
public:

  using base::Quaternion::Quaternion;
  
  quaternion() {
    base::setIdentity();
  }

  static quaternion exp(const vector<3, U>& omega) {
    return {};
  }


  vector<3, U> log() const {
    return vector<3, U>::Zero();    
  }

  // TODO wrap more?
};

using quat = quaternion<real>;


#endif
