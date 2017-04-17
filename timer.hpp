#ifndef TIMING_HPP
#define TIMING_HPP

#include <chrono>

template<class Action, class Resolution = std::chrono::milliseconds,
		 class Clock = std::chrono::high_resolution_clock>
static double timer(const Action& action) {
  typename Clock::time_point start = Clock::now();
  action();
  typename Clock::time_point stop = Clock::now();

  std::chrono::duration<double> res = stop - start;
  return res.count();
}

#endif
 
