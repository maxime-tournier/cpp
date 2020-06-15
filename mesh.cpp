#include "gl.hpp"

#include <QApplication>

#include <QOpenGLWidget>
#include <QMouseEvent>

#include "eigen.hpp"
#include <array>


using namespace eigen;

struct mesh {
  vector<vec3> vertices;
  vector<vec3> normals;
  
  vector<vec2> texcoords;
};


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

  auto use() {
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


  // auto zoom(vec2 pos) {
  //   rigid init = frame;
  //   return [init, pos, this](vec2 current) {
  //     vec2 delta = current - pos;
  //   };
  // }
  
  
};
  
} // namespace gl


struct Viewer: QOpenGLWidget {
  gl::geometry geo;
  gl::camera cam;
  
  void initializeGL() override {
    gl::init();
    static std::vector<std::array<GLfloat, 3>> positions = {
      {0, 0, 0},
      {1, 0, 0},
      {0, -1, 0},
      {0, 1, 0},
      {1, 0, 0},
      {0, 0, 0},      
    };

    geo.vertex.data(positions);
    geo.color.data(positions);

    cam.frame.pos.z() = 2;
  }

  void resizeGL(int width, int height) override {
    glViewport(0, 0, width, height);
    cam.ratio = real(width) / real(height);
  }

  void paintGL() override {
    const auto cam = this->cam.use();
    const auto geo = this->geo.use();
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }


  std::function<void(vec2)> mouse_move;

  vec2 coord(QPoint pos) const {
    return {2 * ((pos.x() / real(width())) - 0.5),
            2 * ((-pos.y() / real(height())) - 0.5)};
  }
  
  void mousePressEvent(QMouseEvent* event) override {
    if(event->buttons() & Qt::LeftButton) {
      mouse_move = cam.pan(coord(event->pos()));
      update();
      return;
    }

    if(event->buttons() & Qt::RightButton) {
      mouse_move = cam.trackball(coord(event->pos()));
      update();
      return;
    }
    
  }
  
  void mouseReleaseEvent(QMouseEvent* event) override {
    mouse_move = 0;
  }
  
  void mouseMoveEvent(QMouseEvent* event) override {
    if(!mouse_move) return;
    mouse_move(coord(event->pos()));
    update();
  }    
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Viewer widget;
  widget.show();
  return app.exec();
}
