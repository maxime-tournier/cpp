#ifndef CORE_RIGID_HPP
#define CORE_RIGID_HPP

#include <core/quat.hpp>

template<class U>
class rigid_transform {
public:
  using center_type = vector<3, U>;
  center_type center;
  
  using orient_type = quaternion<U>;
  orient_type orient;
  

  Eigen::Map< vector<7, U> > coeffs() {
    return Eigen::Map<vector<7, U>>(center.data());
  }

  Eigen::Map< vector<7, const U> > coeffs() const {
    return Eigen::Map<vector<7, U>>(center.data());
  }

  
  rigid_transform() {
    orient.setIdentity();
    center.setZero();
  }


  rigid_transform(const center_type& center,
                  const orient_type& orient)
    : center(center),
      orient(orient) {
    
  }
  

  rigid_transform operator*(const rigid_transform& rhs) const {
    return rigid_transform(center + orient * rhs.center,
                           orient * rhs.orient);
  }

  rigid_transform inv() const {
    orient_type qinv = orient.conjugate();
    return rigid_transform(-(qinv * center), qinv);
  }

  
  center_type operator()(const center_type& x) const {
    return center + orient * x;
  }
  

  // TODO deriv, Ad, exp, log
  
};


using rigid = rigid_transform<real>;

template<class U>
struct traits< rigid_transform<U> > {

  using group_type = rigid_transform<U>;
  using deriv_type = vector<6, U>;


  // R3 x SO(3)
  static group_type id() { return {}; }

  static group_type inv(const group_type& g) {
    return group_type(-g.center, g.orient.conjugate());
  }
  
  static group_type prod(const group_type& g, const group_type& h) {
    return group_type(g.center + h.center,
                      g.orient * h.orient);
  }

  static group_type exp(const deriv_type& dg) {
    return group_type(dg.template head<3>(),
                      quaternion<U>::exp(dg.template tail<3>()));
  }


  static deriv_type AdT(const group_type& g, const deriv_type& x) {
    deriv_type res;
    
    res.template head<3>() = x.template head<3>();
    res.template tail<3>() = g.orient.conjugate() * x.template tail<3>();    

    return res;
  }


  static deriv_type Ad(const group_type& g, const deriv_type& x) {
    deriv_type res;
    
    res.template head<3>() = x.template head<3>();
    res.template tail<3>() = g.orient * x.template tail<3>();    
    
    return res;
  }
  
  

  static const char* name() { return "rigid"; }


  
};



#endif
