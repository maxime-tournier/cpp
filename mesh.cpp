#include "gl.hpp"

#include <QApplication>
#include <QOpenGLWidget>

#include <QMouseEvent>

#include <array>

#include "math.hpp"
#include "camera.hpp"


using namespace math;

struct mesh {
  vector<vec3> vertices;
  vector<vec3> normals;
  
  vector<vec2> texcoords;
};






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
