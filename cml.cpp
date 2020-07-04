#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "maybe.hpp"

namespace event {

// type-erasure class
template<class A>
class any {
  struct base {
    virtual ~base() {}

    virtual maybe<A> poll() = 0;
    virtual A wait() = 0;
  };

  template<class E>
  struct derived: base {
    E impl;
    derived(E impl): impl(std::move(impl)) {}

    maybe<A> poll() override { return impl.poll(); }
    A wait() override { return impl.wait(); }
  };

  std::unique_ptr<base> impl;

public:
  template<class E>
  any(E e): impl(std::make_unique<derived<E>>(std::move(e))) {}

  maybe<A> poll() { return impl->poll(); }
  A wait() { return impl->wait(); }
};


// atomic queue
template<class T>
class queue {
  struct list: std::unique_ptr<std::pair<T, list>> { };
  list data;
  std::mutex mutex;
  using lock_type = std::unique_lock<std::mutex>;

  public:

  template<class Alt>
  maybe<T> pop(Alt alt) {
    const lock_type lock(mutex);
    if(data) {
      maybe<T> result = data->first;
      data = std::move(data->second);
      return result;
    }

    alt();
    return {};
  }

  void push(T value) {
    const lock_type lock(mutex);
    data.reset(new std::pair<T, list>(std::move(value), std::move(data)));
  }
};


    
  
// channel
template<class A>
struct channel {

  enum {
    WAITING,
    CLAIMED,
    SYNCHED
  };
  
  struct side {
    A payload;
    std::condition_variable resume;
    std::atomic<std::size_t> state;
  };
  
  queue<side*> sendq, recvq;
};

template<class A>
static std::shared_ptr<channel<A>> make_channel() {
  return std::make_shared<channel<A>>();
}

  /* 
  - primitive: dequeue waiting with transition
  - once it's been dequeued, nobody else can change its state
  - optimistic phase:
      - dequeue(W->S), then complete tx
  - pessimistic:
    - publish ourselves as waiting, now our state may change
    - recheck
      - try claim our side (W->C)
        - success: now our state may no longer change
          - dequeue(W->S) then:
            - success: 
                - commit our side (C->S), unpublish ourselves
                - complete tx
            - else:
              - rollback (C -> W)
              - recheck
        - else:
          - S: somebody else completed us (hence dequeued us)
             - complete tx
          - C: somebody else is claiming us, retry (cannot happen?)
  */
  
  
template<class A>
class recv {
  using channel_type = std::shared_ptr<channel<A>>;
  channel_type chan;
public:
  recv(recv&&) = default;
  recv(channel_type chan): chan(std::move(chan)) { }
  
  maybe<A> poll() {
    // atomically try pop one waiting sender from the queue
    if(auto some = chan->sendq.pop()) {
      maybe<A> result = std::move(*some.get().source);

      // complete the transaction
      *some.get().done = true;

      // resume sender
      some.get().resume->notify_one();
      
      return result;
    }

    return {};
  }

  A wait() {
    side my;

    my.state = WAITING;
    std::mutex mutex;
    
    // publish ourselves to the recv queue
    chan->recvq.push(&my);

    // recheck send queue to avoid starving
    while(auto some = chan->sendq.pop()) {
      // attempt to claim our side of transaction
      std::size_t expected = WAITING;
      if(my.state.compare_exchange_strong(expected, CLAIMED)) {

        // now attempt to complete transaction
        std::size_t expected = WAITING;        
        if(some.get()->state.compare_exchange_strong(expected, SYNCHED)) {
          // success, complete transaction
          my.result = std::move(some.get()->payload);
        
          // resume other side
          some.get()->resume.notify_one();
        
          // TODO unpublish ourselves from the recvq, atomically
          
          return my.result;
        } else {
          
        }
      } else if(expected == SYNCHED) {
        // some other sender completed our transaction, rollback this one and
        // return
        
        // put back other end
        chand->sendq.push(some.get());
        
        return my.result;
      } else {
        // some other sender is claiming our transaction, rollback
        chand->sendq.push(some.get());
      }
    }
    
    // wait
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return my.state == SYNCHED; });
    
    return my.result;
  }
};

  

struct unit {};

template<class A>
class send {
  using channel_type = std::shared_ptr<channel<A>>;
  channel_type chan;

  A value;

  std::mutex mutex;
  std::condition_variable cv;
public:
  send(send&&) = default;
  send(channel_type chan, A value): chan(std::move(chan)), value(std::move(value)) { }
  
  maybe<unit> poll() {
    // atomically try pop one receiver from the queue
    if(auto ready = chan->recvq.pop()) {
      *ready.get().target = std::move(value);

      // complete the transaction
      *ready.get().done = true;

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

  auto poll() { return map(source.poll(), func); }

  auto wait() { return func(source.wait()); }
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


template<class Event>
static auto sync(Event&& event) {
  if(auto value = event.poll()) {
    return value.get();
  }

  return event.wait();
}

} // namespace event



#include <iostream>

int main(int, char**) {
  using namespace event;
  
  auto chan = make_channel<int>();

  std::thread t1([=] {
    std::cout << sync(recv<int>(chan)) << std::endl;
  });
  
  std::thread t2([=] {
    sync(send<int>(chan, 14));
  });


  t1.join();
  t2.join();
  
  return 0;
}
