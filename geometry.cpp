// -*- compile-command: "c++ -std=c++14 -o geometry geometry.cpp `pkg-config --cflags eigen3` -lstdc++" -*-

#include "group.hpp"
#include "diff.hpp"


int main(int, char**) {
  quat q1, q2;
  
  group<quat> SO3;
  group<vec3> R3;

  group<vec2> R2;
  
  return 0;
}


