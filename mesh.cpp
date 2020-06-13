#include "gl.hpp"

#include <QApplication>
#include <QOpenGLWidget>

#include "eigen.hpp"

using namespace eigen;

struct mesh {
  vector<vec3> vertices;
  vector<vec3> normals;
  vector<vec2> texcoords;

};


struct Viewer: QOpenGLWidget {
  void paintGL() override {
    glBegin(GL_TRIANGLES);
    glVertex3f(0, 0, 0);
    glVertex3f(1, 0, 0);
    glVertex3f(0, 1, 0);        
    glEnd();
  }

  void initializeGL() override {
    
  }

  void resizeGL(int w, int h) override {

  }
  
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Viewer widget;
  widget.show();
  return app.exec();
}
