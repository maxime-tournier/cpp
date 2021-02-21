#ifndef TIMING_HPP
#define TIMING_HPP

#include <chrono>

template<class Action, class Resolution = std::chrono::milliseconds,
		 class Clock = std::chrono::high_resolution_clock>
static double with_time(const Action& action) {
  typename Clock::time_point start = Clock::now();
  action();
  typename Clock::time_point stop = Clock::now();

  std::chrono::duration<double> res = stop - start;
  return res.count();
}


#include <thread>
#include <vector>
#include <atomic>

struct event {
  using clock_type = std::chrono::high_resolution_clock;
  using time_type = clock_type::time_point;
  using id_type = const char*;
  
  id_type id;
  time_type time;
  std::thread::id thread;

  enum { BEGIN, END } kind;

  // named constructors
  static inline event begin(id_type id) {
    return {id, clock_type::now(), std::this_thread::get_id(), BEGIN};
  }
  
  static inline event end(id_type id) {
    return {id, clock_type::now(), std::this_thread::get_id(), END};
  }
};


class timeline {
  std::vector<event> events;
  std::atomic<std::size_t> size;
public:
  timeline(std::size_t size): events(size), size(0) { };

  // add event, return true on success, false otherwise. thread-safe.
  bool add(event ev) {
    // atomic
    const std::size_t index = size++;
    
    if(index >= events.size()) {
      return false;
    }

    events[index] = ev;
    return true;
  }


  // clear timeline. *not* thread-safe
  void clear() {
    const std::size_t max = size;
    events.resize(std::max(max, events.size()));
    size = 0;
  }

  // iterators
  const event* begin() const { return events.data(); }
  const event* end() const { return events.data() + size; }  
  
  static timeline instance;
};

timeline timeline::instance(100000);

struct timer {
  const event::id_type id;
  timeline& tl;
  
  timer(event::id_type id, timeline& tl=timeline::instance): id(id), tl(tl) {
    tl.add(event::begin(id));
  }

  ~timer() { tl.add(event::end(id)); }
};


template<class T>
struct tree: T {
  using T::T;
  std::vector<tree> children;
};

struct node {
  const char* id;
  event::time_type begin, end;
};


// static tree<node> process(const timeline& self) {
//   for(const event& ev: self) {
//     // TODO
//   }
// }


#endif
 
