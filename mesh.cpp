#include "gl.hpp"

#include <QApplication>

#include <QOpenGLWidget>

#include "eigen.hpp"
#include <array>


using namespace eigen;

struct mesh {
  vector<vec3> vertices;
  vector<vec3> normals;
  
  vector<vec2> texcoords;
};


namespace gl {

struct camera {
  vec3 pos;
  quat orient;

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

    const mat3x3 RT = orient.conjugate().matrix();
    
    res <<
      RT.cast<GLfloat>(), -(RT * pos).cast<GLfloat>(),
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
  
};
  
} // namespace gl


struct Viewer: QOpenGLWidget {
  gl::geometry geo;
  gl::camera cam;
  
  void initializeGL() override {
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

    cam.pos.z() = 2;
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
  
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Viewer widget;
  widget.show();
  return app.exec();
}
