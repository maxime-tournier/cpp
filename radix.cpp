// -*- compile-command: "c++ -std=c++11 -o radix radix.cpp -g -lstdc++" -*-

#include "radix.hpp"


int main(int, char**) {
  using vec = vector<double, 2>;

  vec v;

  for(std::size_t i = 0; i < 17; ++i) {
    v = v.push_back(i);
  }

  std::clog << v << std::endl;
  return 0;
}
