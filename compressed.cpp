// -*- compile-command: "c++ -std=c++14 compressed.cpp -o hamt -lstdc++ -g" -*-

#include "compressed.hpp"

int main(int, char**) {
  using value_type = double;
  auto array = std::make_shared<storage<value_type, 10, 1>>(1, value_type{5}); 

  array->set(0, 10);
  
  return 0;
}
