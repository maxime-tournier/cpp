#ifndef TIMING_HPP
#define TIMING_HPP

#include <chrono>

template<class Action, class Resolution = std::chrono::milliseconds,
		 class Clock = std::chrono::high_resolution_clock>
static double time(const Action& action) {
  typename Clock::time_point start = Clock::now();
  action();
  typename Clock::time_point stop = Clock::now();

  std::chrono::duration<double> res = stop - start;
  return res.count();
}

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
 
