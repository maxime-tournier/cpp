// -*- compile-command: "c++ -O3  -o task task.cpp -lstdc++ -lpthread" -*-

#include "task.hpp"

#include <chrono>
#include <thread>


int fib(int n) {
  if(n <= 1) return n;
  return fib(n - 1) + fib(n - 2);
}



int main(int, char**) {

  pool p;

  auto fut = p.split(0ul, 8ul, [&](int i) {
      // std::this_thread::sleep_for(std::chrono::seconds(2));
      std::clog << fib(45) << std::endl;
    });
  
  fut.get();
  
  return 0;
}
