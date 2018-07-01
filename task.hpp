#ifndef CPP_TASK_HPP
#define CPP_TASK_HPP

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

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
  bool stop = false;

  lock_type lock() { return lock_type(mutex); }
  lock_type try_lock() { return lock_type(mutex, std::try_to_lock); }
  
public:

  void done() {
    {
      const auto lock = this->lock();
      stop = true;
    }

    cv.notify_all();
  }


  bool try_pop(task_type& out) {
    auto lock = this->try_lock();
    if(!lock || tasks.empty()) return false;

    out = std::move(tasks.front());
    tasks.pop_front();
    return true;    
  }
  
  bool pop(task_type& out) {
    auto lock = this->lock();

    cv.wait(lock, [this]  { return tasks.size() || stop; });
    if(tasks.empty()) return false;
    
    out = std::move(tasks.front());
    tasks.pop_front();
    return true;
  };


  bool try_push(task_type task) {
    {
      const auto lock = this->try_lock();
      if(!lock) return false;
      tasks.emplace_back(std::move(task));
    }

    cv.notify_one();
    return true;
  }

  

  void push(task_type task) {
    {
      const auto lock = this->lock();
      tasks.emplace_back(std::move(task));
    }
    cv.notify_one();
  }


  
};



class pool {
  std::atomic<int> last;
  std::vector<queue> queues;
  std::vector<std::thread> threads;

  const double factor = 1.5;
  
  void run(std::size_t i) {
      
    while(true) {
      task_type task;
      
      // try stealing work from someone else's queue (starting from our own)
      // instead of waiting for work
      for(std::size_t j = 0, n = size(); j < n; ++j) {
        if(queues[(i + j) % n].try_pop(task)) break;
      }

      // no work available: fallback on waiting
      if(!task && !queues[i].pop(task)) {
        return;
      }
      
      task();
    }
  }

public:
  std::size_t size() const { return threads.size(); }
  
  pool(std::size_t n = std::thread::hardware_concurrency())
    : last(0),
      queues(n) {
    
    for(std::size_t i = 0; i < n; ++i) {
      threads.emplace_back([this, i] {
          run(i);
        });
    }
  }


  ~pool() {
    for(auto& q : queues) {
      q.done();
    }
    
    for(auto& t : threads) {
      t.join();
    }
  }


  void async(task_type task) {
    const int next = last++;

    // try pushing on a queue that is not locked to avoid contention
    for(std::size_t i = 0, n = factor * size(); i < n; ++i) {
      if(queues[(next + i) % size()].try_push(std::move(task))) {
        return;
      }
    }

    // all the queues are busy: fallback on waiting
    queues[next % size()].push(std::move(task));
  }
  
};



#endif
