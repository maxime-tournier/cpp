// -*- compile-command: "c++ -o octree octree.cpp -Wall -g -L. -lviewer -lGL -lGLU -lstdc++ `pkg-config --cflags eigen3`" -*-

#include "octree.hpp"


#include <iostream>

#include "viewer.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

using vec3 = Eigen::Matrix<real, 3, 1>;

auto quadric = gluNewQuadric();

namespace gl {
  

  static void draw_cube(GLenum mode) {
    // front
    glBegin(mode);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);
    glEnd();

    // back
    glBegin(mode);    
    glVertex3f(0.0f, 0.0f, 1.0f);
    glVertex3f(1.0f, 0.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 1.0f);
    glEnd();    

    // right
    glBegin(mode);        
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glEnd();

    // left
    glBegin(mode);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);
    glEnd();

    // top
    glBegin(mode);
    glVertex3f(0.0f, 1.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 1.0f);
    glEnd();

    // bottom
    glBegin(mode);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glEnd();
  }


  struct context {
    const GLenum mode;
    context(GLenum mode = GL_MODELVIEW) : mode(mode) {
      glMatrixMode(mode);
      glPushMatrix();
    }
    ~context() {
      glMatrixMode(mode);
      glPopMatrix();
    }
  };

  void translate(const vec3& t) {
    glTranslated(t.x(), t.y(), t.z());
  }

  void scale(const vec3& s) {
    glScaled(s.x(), s.y(), s.z());
  }
  
  static void draw_cell(vec3 start, vec3 end) {
    const auto ctx = context();
    translate(start);
    scale(end - start);
    draw_cube(GL_LINE_LOOP);
  }

}

  

int main(int argc, char** argv) {

  using ucell = cell<unsigned long>;
  
  ucell test(2, 2, 2);


  const auto write = [](std::ostream& out) {
    std::size_t count = 0;
    return [&out, count](ucell::coord x, ucell::coord y, ucell::coord z) mutable {
      out << count++ << ": " << x.to_ulong() << " " << y.to_ulong() << " " << z.to_ulong() << std::endl;
    };
  };

  ucell(1, 1, 1).neighbors(0, write(std::clog));  
  std::clog << "end" << std::endl;
  
  ucell(0, 1, 1).neighbors(0, write(std::clog));  
  std::clog << "end" << std::endl;  

  ucell(0, 0, 1).neighbors(0, write(std::clog));  
  std::clog << "end" << std::endl;  
 
  ucell(0, 0, 0).neighbors(0, write(std::clog));  
  std::clog << "end" << std::endl;  

  ucell(2, 2, 2).neighbors(1, write(std::clog));  
  std::clog << "end" << std::endl;
  
  // test.decode([](ucell::coord x, ucell::coord y, ucell::coord z) {
  //     std::clog << x.to_ulong() << " " << y.to_ulong() << " " << z.to_ulong() << std::endl;
  //   });

  octree<unsigned long> tree;

  const std::size_t n = 10;
  for(std::size_t i = 0; i < n; ++i) {
    const vec3 p = vec3::Random().array().abs();
    tree.push(p);
  }

  const vec3 query = vec3::Random();

  const auto results = [&](const vec3* p) {
    if(p) {
      std::clog << "nearest: " << p->transpose() << ", distance: " << (query - *p).norm() << std::endl;
    } else {
      std::clog << "error!" << std::endl;
    }
  };
  
  results(tree.nearest(query));
  results(tree.brute_force(query));  

  viewer w;

  w.draw = [&] {
    glDisable(GL_LIGHTING);
    gl::draw_cell(vec3::Zero(), vec3::Ones());
  };

  return w.run(argc, argv);
}


