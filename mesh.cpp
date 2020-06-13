
#include <QApplication>

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>

#include "eigen.hpp"
#include <array>

#include "gl.hpp"

using namespace eigen;

struct mesh {
  vector<vec3> vertices;
  vector<vec3> normals;
  
  vector<vec2> texcoords;
};


struct Viewer: QOpenGLWidget {
  gl::geometry geo;
  QOpenGLShaderProgram program;
  
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
  }

  void resizeGL(int w, int h) override { }

  void paintGL() override {
    geo.enable();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Viewer widget;
  widget.show();
  return app.exec();
}
