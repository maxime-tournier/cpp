#include "vm.hpp"
#include <vector>
#include <iostream>

using namespace vm;

static std::vector<code> test = 
{
 push, word(42),
 ret
};


static std::vector<code> id = 
{
 push, word(42),
 call, word(3), word(1),
 
 load, word(-1),
 ret
};





  
int main(int, char**) {
  static constexpr std::size_t size = 1024;
  static vm::word stack[size];

  std::cout << vm::eval(id.data(), stack) << std::endl;
  
  return 0;
}
