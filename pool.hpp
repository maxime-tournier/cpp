#ifndef CPP_POOL_HPP
#define CPP_POOL_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>

// TODO remove this one?
#include <iostream>


// a simple work queue with worker threads
class pool {
  std::vector< std::thread > thread;

  std::mutex mutex;
  std::condition_variable cv;

  struct exit { };
  
  using task_type = std::function< void() >;
  std::queue<task_type> queue;
  
  // obtain a task
  task_type pop() {
    std::unique_lock<std::mutex> lock(mutex);
    
    cv.wait(lock, [this]{ return !queue.empty(); } );
    task_type task = queue.front();
    queue.pop();
    return task;
  }

  static std::mutex& debug_mutex() {
    static std::mutex mutex;
    return mutex;
  }
  
public:
  
  template<class ... Args>
  static void debug(Args&& ... args) {
    std::unique_lock<std::mutex> lock(debug_mutex());
    std::ostream& out = std::clog;
    out << std::hex << "[" << std::this_thread::get_id() << "][" << sched_getcpu() <<  "]"<< std::dec;
    (void) std::initializer_list<int> { (out << " " << args, 0)... };
    out << std::endl;
  };

  // construct a pool with n threads
  pool(std::size_t n = std::thread::hardware_concurrency() ) {
    if(n == 0) throw std::runtime_error("no threads");
    
    for(std::size_t i = 0; i < n; ++i) {
      thread.emplace_back( [this] {
          try {
            while(true) {
              task_type task = pop();
              task();
            }
          } catch( exit ) {
            debug("exit");
          }
        });
    }
  }

  ~pool() {

    for(std::size_t i = 0, n = thread.size(); i < n; ++i) {
      push( [] { throw exit(); } );
    }

    for(std::thread& t : thread) {
      t.join();
    }
  }

  // append a task
  void push(task_type&& task) {
    {
      std::unique_lock<std::mutex> lock(mutex);
      queue.emplace( std::move(task) );
    }

    cv.notify_one();
  }


};


#endif
