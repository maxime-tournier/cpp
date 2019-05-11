// -*- compile-command: "CXXFLAGS=-std=c++14 make amt" -*-

#include "amt.hpp"
#include <iostream>

int main(int, char**) {

  array<double, 8, 7> test;
  test = test.set(0, 1.0); 
  std::clog << test.get(0) << std::endl;
  return 0;
}
 
  
 
 
