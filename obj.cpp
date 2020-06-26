// -*- compile-command: "c++ -std=c++14 obj.cpp -o obj -lstdc++ -lm -g" -*-
#include "obj.hpp"

#include <fstream>
#include <iostream>



int main(int argc, char** argv) {
  obj::file f;

  if(argc <= 1) {
    std::cerr << "usage: " << argv[0] << " <objfile>" << std::endl;
    return 1;
  }
  const std::string filename = argv[1];
  
  std::ifstream in(filename);
  if(!in) {
    std::cerr << "cannot read: " << filename << std::endl;
    return 1;
  }

  try {
    in >> f;
    std::cout << "read " << filename << std::endl;
  } catch(std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
