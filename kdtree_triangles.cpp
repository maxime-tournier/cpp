// -*- compile-command: "c++ -std=c++14 -O3 -DNDEBUG -ffast-math -march=native kdtree_triangles.cpp -o kdtree_triangles `pkg-config --cflags eigen3` -lstdc++" -*-

#include "kdtree_triangles.hpp"
#include <Eigen/Geometry>


using real = double;

using vec3 = Eigen::Matrix<real, 3, 1>;
using quat = Eigen::Quaternion<real>;

using vec3i = Eigen::Matrix<Eigen::Index, 3, 1>
using vec2i = Eigen::Matrix<Eigen::Index, 2, 1>;

static void unit_tetrahedron(std::vector<vec3>& points, std::vector<vec3i>& triangles) {
  points.clear();
  triangles.clear();
  
  const quat q1(Eigen::AngleAxis<real>(2 * M_PI / 3, vec3::UnitY()));
  const quat q2(Eigen::AngleAxis<real>(1 * M_PI / 3, vec3::UnitZ()));

  const quat q = q2 * q1;

  vec3 a = vec3::UnitX();
  vec3 b = q * a;
  vec3 c = q * b;

  const quat r = q2.conjugate() * q1;
  vec3 d = r * a;

  points = {a, b, c, d};

  triangles = {{0, 2, 1},
               {1, 2, 3},
               {3, 0, 1},
               {2, 0, 3}};               
}

struct geometry {
  std::vector<vec3> vertices;
  std::vector<vec3i> triangles;
};


static geometry subdivide(geometry g, std::size_t n) {
  std::map<vec2i, std::size_t> edges;
  for(auto tri: g.triangles) {
    for(auto e: {{tri
  }
}
                      



int main(int, char**) {
  
  std::vector<vec3> points;
  std::ve
  
  return 0;
}
