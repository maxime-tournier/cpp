#include "vm.hpp"
#include "as.hpp"

#include <vector>
#include <set>
#include <map>

#include <iostream>


extern std::vector<as::line> fib;

int main(int, char**) {
  static constexpr std::size_t size = 1024;
  static vm::word stack[size];

  const auto prog = as::link(fib);
  std::cout << vm::eval(prog.data(), stack).value << std::endl;
  
  return 0;
}
 
 
 
