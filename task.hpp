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
  
public:

  void done() {
    {
      const auto lock = this->lock();
      stop = true;
    }

    cv.notify_all();
  }
  
  bool pop(task_type& out) {
    auto lock = this->lock();

    cv.wait(lock, [this]  { return tasks.size() || stop; });
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
  std::atomic<int> last;
  std::vector<queue> queues;
  std::vector<std::thread> threads;
  
  void run(std::size_t i) {
    task_type task;
      
    while(queues[i].pop(task)) {
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
    queues[next % size()].push(std::move(task));
  }
  
};



#endif
