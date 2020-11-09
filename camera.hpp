#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "gl.hpp"
#include "math.hpp"

namespace gl {

struct camera {
  rigid frame;
  vec3 pivot;
  
  real fovy = M_PI / 3;
  real ratio = 1.0;
  real znear = 0, zfar = 10;

  using mat4x4 = matrix<GLfloat, 4, 4>;
  
  mat4x4 projection() const {
    mat4x4 res;
    
    const real half_fovy = fovy / 2;
    const real f = std::cos(half_fovy) / std::sin(half_fovy);
    const real zsum = zfar + znear;
    const real zdiff = zfar - znear;
    res <<
      f / ratio, 0, 0, 0,
      0, f, 0, 0, 
      0, 0, zsum / zdiff, 2 * znear * zfar / zdiff,
      0, 0, -1, 0;

    return res;
  }
  
  mat4x4 modelview() const {
    mat4x4 res;

    const mat3x3 RT = frame.orient.conjugate().matrix();
    
    res <<
      RT.cast<GLfloat>(), -(RT * frame.pos).cast<GLfloat>(),
      0, 0, 0, 1;

    return res;
  }
  

  void enable() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(projection().data());

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(modelview().data());    
  }

  
  void disable() {
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
  }

  auto lock() {
    enable();
    return finally([this] { disable(); });
  }

  auto pan(vec2 pos) {
    const rigid init = frame;
    return [init, pos, this](vec2 current) {
      const vec2 delta = current - pos;
      frame = init * rigid::translation(delta.x(), delta.y(), 0).inv();
    };
  }

  auto trackball(vec2 pos) {
    const rigid init = frame;
    return [init, pos, this](vec2 current) {
      const vec2 delta = current - pos;
      const rigid t = rigid::translation(pivot);
      
      const vec3 omega =
        (pivot - init.pos).cross(init.orient * vec3(delta.x(), delta.y(), 0));
      
      const real theta = omega.norm();
      const real eps = 1e-8;
      const real sinc = (theta < eps) ? 1.0 / 2.0 : std::sin(theta / 2) / theta;
      
      quat q;
      q.w() = std::cos(theta / 2);
      q.vec() = omega * sinc;
      
      frame = t * rigid::rotation(q) * t.inv() * init;
    };
  }

};
  
} // namespace gl



#endif
