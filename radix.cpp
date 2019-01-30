// -*- compile-command: "c++ -std=c++11 -o radix radix.cpp -O3 -DNDEBUG -g -lstdc++" -*-

#include "radix.hpp"

#include <deque>
#include <vector>

int main(int, char**) {
  using vec = vector<double, 8, 9>;

  vec v;

  const std::size_t n = 20000000;
  // std::vector<double> test;
  
  for(std::size_t i = 0; i < n; ++i) {
    // v = v.push_back(i);
    v = std::move(v).push_back(i);
    // test.push_back(i);
  }

  // std::clog << v.size() << std::endl;
  // std::clog << test.size() << std::endl;  
  return 0;
}
