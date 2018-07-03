// -*- compile-command: "c++ -o octree octree.cpp -Wall -g -lstdc++ `pkg-config --cflags eigen3`" -*-

#include "octree.hpp"


#include <iostream>


using vec3 = Eigen::Matrix<real, 3, 1>;




int main(int, char** ) {

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
    tree.push(vec3::Random());
  }

  const vec3 query = vec3::Random();
  
  if(auto* p = tree.nearest(query)) {
    std::clog << "nearest: " << p->transpose() << std::endl;
  } else {
    std::clog << "error!" << std::endl;
  }

  if(auto* p = tree.brute_force(query)) {
    std::clog << "reference: " << p->transpose() << std::endl;
  } else {
    std::clog << "error!" << std::endl;
  }
  
  return 0;
}


