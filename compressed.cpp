// -*- compile-command: "c++ -std=c++14 compressed.cpp -o compressed -lstdc++ -g" -*-

#include "compressed.hpp"
#include <iostream>

int main(int, char**) {
  using value_type = double;
  std::shared_ptr<array<value_type>> array = std::make_shared<storage<value_type, 10, 1>>(1, value_type{5}); 
  
  array = array->set(0, 10);
  array = array->set(4, 14);  
  std::clog << array->get(4) << std::endl;
  return 0;
}
