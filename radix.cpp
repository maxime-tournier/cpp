// -*- compile-command: "c++ -std=c++11 -o radix radix.cpp -O3 -DNDEBUG -lstdc++" -*-

#include "radix.hpp"


int main(int, char**) {
  using vec = vector<double, 7>;

  vec v;

  const std::size_t n = 20000000;
  
  for(std::size_t i = 0; i < n; ++i) {
    // v = v.push_back(i);
    v = std::move(v).push_back(i);    
  }

  std::clog << v.size() << std::endl;
  return 0;
}
