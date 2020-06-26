#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "maybe.hpp"

namespace event {

  template<class A>
  class any {
    struct base {
      virtual ~base() { }

      virtual maybe<A> poll() = 0;
      virtual A wait() = 0;
    };

    template<class E>
    struct derived: base {
      E impl;
      derived(E impl): impl(std::move(e)) { }
      
      maybe<A> poll() override { return impl.poll(); }
      A wait() override { return impl.wait(); }      
    };
    
    std::unique_ptr<base> impl;
  public:
    template<class E>
    any(E e): impl(std::make_unique<derived<E>(std::move(e))) { } 
    
    maybe<A> poll() { return impl->poll(); }
    A wait() { return impl->wait(); }    
  };

  
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
      bool* done;
    };
    
    queue<sender> sendq;
    
    struct receiver {
      A* target;      
      std::condition_variable* resume;
      bool* done;
    };

    queue<receiver> recvq;
  };

  template<class A>
  static std::shared_ptr<A> make_channel() {
    return std::make_shared<channel<A>>();
  }

  template<class A>
  struct recv {
    std::shared_ptr<channel<A>> chan;

    std::mutex mutex;
    std::condition_variable cv;

    maybe<A> poll() {
      // atomically try pop one sender from the queue
      if(auto ready = chan->sendq.pop()) {
        maybe<A> result = std::move(*ready.get().source);
        
        // complete the transaction
        ready.get().done = true;
        
        // resume sender
        ready.get().resume->notify_one();
        
        return result;
      }
      
      return {};
    }

    A wait() {
      // allocate target value
      A result;
      
      bool done = false;
      
      // add ourselves to the recv queue
      chan->recvq.push({&result, &cv, &done});
      
      // TODO recheck?
      
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [&] { return done; });

      return result;
    }
  };

  struct unit {};

  template<class A>
  struct send {
    A value;
    std::shared_ptr<channel<A>> chan;
    
    std::mutex mutex;
    std::condition_variable cv;

    maybe<unit> poll()  {
      // atomically try pop one receiver from the queue
      if(auto ready = chan->recvq.pop()) {
        *ready.get().target = std::move(value);
        
        // complete the transaction
        ready.get().done = true;
        
        // resume receiver
        ready.get().resume->notify_one();
        
        return unit{};
      }
      
      return {};
    }

    unit wait() {
      bool done = false;
      
      // add ourselves to the send queue
      chan->recvq.push({&value, &cv, &done});
      
      // TODO recheck?
      
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [&] { return done; });

      return {};
    }
    
  };


  template<class Ev, class Func>
  struct map_type {
    Ev source;
    const Func func;

    auto poll() {
      return map(source.poll(), func);
    }

    auto wait() {
      return func(source.wait());
    }
  };

  template<class Ev, class Func>
  static map_type<Ev, Func> map(Ev ev, Func func) {
    return {std::move(ev), std::move(func)};
  }

  
  template<class A>
  struct choose {
    std::vector<any<A>> events;
    
    maybe<A> poll() {
      // TODO randomize 
      for(const auto& event: events) {
        if(auto ready = event.poll()) {
          return ready;
        }
      }
    }

    A wait() {
      // ????

    }
    
  };
  
  
}



int main(int, char**) {
  return 0;
}
