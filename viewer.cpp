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
      if(owner->draw) owner->draw();
    }

    void init() {
      if(owner->init) owner->init();
    }

    void animate() {
      if(owner->animate) owner->animate();
    }
    
  };
}
      
 
int viewer::run(int argc, char** argv) {
  QApplication app(argc, argv);

  Viewer w(this);
  w.show();
  
  return app.exec();
}


