// -*- compile-command: "c++ -g -O3 -DNDEBUG -o octree octree.cpp -Wall -L. -lviewer -lGL -lGLU -lstdc++ `pkg-config --cflags eigen3`" -*-

#include "octree.hpp"


#include <iostream>

#include "viewer.hpp"

#include <GL/gl.h>
#include <GL/glu.h>

#include "timer.hpp"
#include "kdtree.hpp"


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

  // source data

  std::vector<vec3> data;

  real duration = timer([&] {
      const std::size_t n = 5000000;
      data.reserve(n);
      for(std::size_t i = 0; i < n; ++i) {
        data.emplace_back(vec3::Random().array().abs());
      }
    });
  std::clog << "generate: " << duration << std::endl;
  
  
  const vec3 query = vec3::Random().array().abs();

  const auto results = [&](const vec3* p) {
    if(p) {
      std::clog << "nearest: " << p->transpose() << ", distance: " << (query - *p).norm() << std::endl;
    } else {
      std::clog << "error!" << std::endl;
    }

  };

  
  const vec3* res;

  
  octree<unsigned long> tree;

  const real octree_prepare = timer( [&] {
      tree.reserve(data.size());

      for(const vec3& p : data) {
        tree.push(p);
      }

      tree.sort();      
    });
  
  const real octree_find = timer([&] {
      res = tree.nearest(query);
    });

  std::clog << "octree:" << std::endl;  
  results(res);
  std::clog << "octree prepare: " << octree_prepare << std::endl;
  std::clog << "octree find: " << octree_find << std::endl;;
  std::clog << "octree total: " << octree_find + octree_prepare << std::endl;;  
  
  
  kdtree kd;

  const real kdtree_prepare = timer([&] {
      kd.build(data.begin(), data.end());
    });

  const real kdtree_find = timer([&] {
      auto index = kd.closest(query, data.begin(), data.end());
      res = &data[index];
    });

  std::clog << "kdtree:" << std::endl;  
  results(res);
  std::clog << "kdtree prepare: " << kdtree_prepare << std::endl;
  std::clog << "kdtree find: " << kdtree_find << std::endl;;
  std::clog << "kdtree total: " << kdtree_find + kdtree_prepare << std::endl;;  

  std::clog << "brute force:" << std::endl;
  const real brute_force = timer([&] {
      res = tree.brute_force(query);
    });
  
  results(res);
  std::clog << "brute force find: " << brute_force << std::endl;


  
  
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

    // const vec3 p = tree.origin(c);
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


