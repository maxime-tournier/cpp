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


  static constexpr U epsilon = std::numeric_limits<U>::epsilon();

  
  static quaternion exp(const vector<3, U>& omega) {
    const U theta = omega.norm();

    quaternion res;

    if(theta < epsilon) {
      res.vec() = omega;
      res.normalize();
    } else {
      const U half_theta = theta / 2;
      res.w() = std::cos(half_theta);
      res.vec() = (std::sin(half_theta) / theta) * omega;
    }
    
    return res;
  }


  vector<3, U> log() const {
    quaternion q;

    if(this->w() < 0) {
      q = - *this;
    } else {
      q = *this;
    }

    const U half_theta = std::acos( std::min(q.w(), 1.0) );

    if(half_theta < epsilon) {
      return (q / q.w()).vec();
    } else {
      const U theta = 2 * theta;
      return q.vec() * ( theta / std::sin(half_theta));
    }
    
  }

};


using quat = quaternion<real>;


#endif
