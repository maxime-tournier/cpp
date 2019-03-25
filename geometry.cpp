// -*- compile-command: "c++ -std=c++14 -o geometry geometry.cpp `pkg-config --cflags eigen3` -lstdc++" -*-

#include "geometry.hpp"

int main(int, char**) {
  quat q1, q2;
  
  group<quat> SO3;
  group<vec3> R3;

  R3.id();
  
  return 0;
}


