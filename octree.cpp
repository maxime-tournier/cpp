// -*- compile-command: "c++ -g -O3 -o octree octree.cpp -Wall -L. -lviewer -lGL -lGLU -lstdc++ `pkg-config --cflags eigen3`" -*-

#include "octree.hpp"


#include <iostream>

#include "viewer.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

#include "timer.hpp"

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

  struct begin {
    begin(GLenum mode) {
      glBegin(mode);
    }
    
    ~begin() {
      glEnd();
    }
  };

  
  void translate(const vec3& t) {
    glTranslated(t.x(), t.y(), t.z());
  }

  void color(const vec3& c) {
    glColor3d(c.x(), c.y(), c.z());
  }

  void vertex(const vec3& c) {
    glVertex3d(c.x(), c.y(), c.z());
  }
  
  
  void scale(const vec3& s) {
    glScaled(s.x(), s.y(), s.z());
  }
  
  static void draw_aabb(vec3 lower, vec3 upper) {
    const auto ctx = context();
    translate(lower);
    scale(upper - lower);
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

  double duration = timer( [&] {
      const std::size_t n = 5000000;
      tree.reserve(n);
      for(std::size_t i = 0; i < n; ++i) {
        const vec3 p = vec3::Random().array().abs();
        tree.push(p);
      }
    });
  std::clog << "filling: " << duration << std::endl;
  
  const vec3 query = vec3::Random().array().abs();

  const auto results = [&](const vec3* p, real duration) {
    if(p) {
      std::clog << "duration: " << duration << std::endl;
      std::clog << "nearest: " << p->transpose() << ", distance: " << (query - *p).norm() << std::endl;
    } else {
      std::clog << "error!" << std::endl;
    }
  };

  tree.sort();

  
  const vec3* res;
  
  duration = timer([&] {
      res = tree.nearest(query);
    });
  
  results(res, duration);

  duration = timer([&] {
      res = tree.brute_force(query);
    });
  
  results(res, duration);

  viewer w;


  w.draw = [&] {
    glDisable(GL_LIGHTING);

    gl::color(vec3::Ones());

    ucell c = tree.hash(query);
    
    for(std::size_t i = 0; i <= ucell::max_level; ++i) {
      unsigned long mask = ~((1ul << (3 * i)) - 1ul);

      // std::clog << "i: " << i << " mask: " << ucell(mask).bits << std::endl;
      
      ucell p(c.bits.to_ulong() & mask);
      // std::clog << "c: " << c.bits << std::endl;
      // std::clog << "p: " << p.bits << std::endl;
      // std::clog << "p: " << p << std::endl;      

      
      const real size = real(1ul << i) / ucell::resolution();
      const vec3 lower = tree.origin(p);
      const vec3 upper = lower + size * vec3::Ones();

      // std::clog << "lower: " << lower.transpose() << std::endl;
      // std::clog << "upper: " << upper.transpose() << std::endl;      
      
      gl::draw_aabb(lower, upper);
    }

    const vec3 p = tree.origin(c);
    // gl::draw_aabb(p - 0.1 * vec3::Ones(), p + 0.1 * vec3::Ones());
    // gl::draw_aabb(vec3::Zero(), vec3::Ones());

    gl::color({1, 0, 0});
    glPointSize(4);

    {
      auto ctx = gl::begin(GL_POINTS);
      gl::vertex(query);
    }
    
  };

  return 0; // w.run(argc, argv);
}


