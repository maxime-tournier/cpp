#ifndef SPARSE_TIMER_HPP
#define SPARSE_TIMER_HPP

#include <chrono>

struct timer {
  using clock = std::chrono::high_resolution_clock;
  clock::time_point last;

  double restart() {
    clock::time_point now = clock::now();
    std::chrono::duration<double> delta = now - last;
    last = now;
    return delta.count();
  }
  
};


#endif
