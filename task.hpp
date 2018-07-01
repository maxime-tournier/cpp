#ifndef CPP_TASK_HPP
#define CPP_TASK_HPP

#include <mutex>
#include <condition_variable>
#include <thread>

#include <deque>
#include <vector>

#include <functional>


#include <iostream>

using mutex_type = std::mutex;
using lock_type = std::unique_lock<mutex_type>;

using task_type = std::function< void() >;

class queue {
  std::deque<task_type> tasks;
  mutex_type mutex;
  std::condition_variable cv;
  bool done = false;

  lock_type lock() { return lock_type(mutex); }
  
public:

  void finish() {
    std::clog << "finishing" << std::endl;
    
    {
      const auto lock = this->lock();
      done = true;
    }

    cv.notify_all();
  }
  
  bool pop(task_type& out) {
    auto lock = this->lock();
    cv.wait(lock, [this] { return tasks.size() && !done; });
    if(tasks.empty()) return false;
    
    out = std::move(tasks.front());
    tasks.pop_front();
    return true;
  };


  void push(task_type task) {
    {
      const auto lock = this->lock();
      tasks.emplace_back(std::move(task));
    }
    cv.notify_one();
  }
  
};



class pool {
  std::vector<std::thread> threads;
  queue q;
  
  void run(std::size_t i) {
    std::clog << "thread " << i << " started" << std::endl;

    task_type task;
      
    while(q.pop(task)) {
      task();
    }
  }

public:
  pool(std::size_t n = std::thread::hardware_concurrency()) {
    for(std::size_t i = 0; i < n; ++i) {
      threads.emplace_back([this, i] { run(i); });
    }
  }


  ~pool() {
    q.finish();
    
    for(auto& t : threads) {
      t.join();
    }
  }


  void async(task_type task) {
    q.push(std::move(task));
  }
  
};



#endif
