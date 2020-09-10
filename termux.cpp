#include <iostream>

int main() {
  std::cout << "cout" << std::endl;
  std::cerr << "cerr" << std::endl;
  std::clog << "clog" << std::endl;    

  try {
    throw std::runtime_error("error");
  } catch(std::runtime_error& e) {
    std::clog << e.what() << std::endl;;
  }
  
  return 0;
}
