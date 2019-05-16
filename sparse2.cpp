#include "sparse2.hpp"

int main() {

  array<double> test;
  test = test.set(0, 2.0);
  test = test.set(10, 4.0);  
  std::clog << test.get(0) << std::endl;
  std::clog << test.get(10) << std::endl;
  std::clog << test.size() << std::endl;
  return 0;
}
