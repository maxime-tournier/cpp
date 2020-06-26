#include <thread>
#include <condition_variable>
#include <mutex>

#include "maybe.hpp"

namespace event {

  template<class T>
  class queue {
    struct list: std::unique_ptr<std::pair<T, list>> { };
    list data;
    std::mutex mutex;
    using lock_type = std::unique_lock<std::mutex>;    
  public:
    maybe<T> pop() {
      const lock_type lock(mutex);
      return data.pop();
    }

    void push(T value) {
      const lock_type lock(mutex);
      data.push(std::move(value));
    }
  };
  
  template<class A>
  struct channel {
    struct sender {
      A* source;
      std::condition_variable* resume;
      bool* finished;      
    };
    
    queue<sender> sendq;
    
    struct receiver {
      A* target;      
      std::condition_variable* resume;
    };

    queue<receiver> recvq;
  };


  template<class A>
  struct event {
    virtual maybe<A> poll() = 0;
    virtual A wait() = 0;
  };
  
  template<class A>
  struct send: event<void> {
    std::shared_ptr<channel<A>> chan;

    std::mutex mutex;
    std::condition_variable cv;
  };


  template<class A>
  struct recv: event<A> {
    std::shared_ptr<channel<A>> chan;

    std::mutex mutex;
    std::condition_variable cv;

    maybe<A> poll() override {
      // atomically try pop one sender in the queue
      if(auto ready = chan->sendq.pop()) {
        maybe<A> result = std::move(*ready.get().source);
        
        // reset source pointer, completing the transaction
        ready.get().done = nullptr;        
        
        // resume sender
        ready.get().resume->notify_one();
        return result;
      }
      
      return {};
    }

    A wait() override {
      // allocate target value
      maybe<A> result;

      bool done = false;
      
      // add ourselves to the recv queue
      chan->recvq.push({&result, &cv, &done});
      
      // TODO recheck?
      
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [&] { return done; });
    }
  };



  

  
}



int main(int, char**) {
  return 0;
}
