// -*- compile-command: "CXXFLAGS='-std=c++14 -g' make sparse_array" -*-

#include "sparse_array.hpp"


int main(int, char**) {
  using type = double;

  auto d = std::make_shared<derived<type>>();
  ref<base<type>> b = d;

  auto p = b->set(7, 2.0);
  std::cout << *p << std::endl;
  
  auto q = p->set(2, 1.0);
  std::cout << *q << std::endl;    

  
  return 0;
}
