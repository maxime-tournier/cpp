#include "viewer.hpp"

#include <QGLViewer/qglviewer.h>
#include <QApplication>

namespace {
  struct Viewer :  QGLViewer {
    viewer* owner;
    Viewer(viewer* owner, QWidget* parent = nullptr)
      : QGLViewer(parent),
        owner(owner) {

    }
    
    void draw() {
      if(owner->on_draw) owner->on_draw();
    }

    void init() {
      if(owner->on_init) owner->on_init();
    }

    void animate() {
      if(owner->on_animate) owner->on_animate();
    }
    
  };
}
      
 
int viewer::run(int argc, char** argv) {
  QApplication app(argc, argv);

  Viewer w(this);
  w.show();
  
  return app.exec();
}


