#ifndef MAYBE_HPP
#define MAYBE_HPP

#include <type_traits>
#include <cassert>
#include <new>

// maybe monad
template<class T>
class maybe {
  using storage_type = typename std::aligned_union<0, T>::type;
  storage_type storage;
  
  const bool set;

public:
  using value_type = T;

  // monadic return (none/just)
  maybe(): set(false) {}
  maybe(T value): set(true) { new(&storage) T{std::move(value)}; }

  maybe(maybe&& other): set(other.set) {
    if(set) {
      new(&storage) T{std::move(other.get())};
    }
  }

  maybe(const maybe& other): set(other.set) {
    if(set) {
      new(&storage) T{other.get()};
    }
  }

  explicit operator bool() const { return set; }  

  // unclear whether we actually need these at all
  maybe& operator=(maybe&& other) = delete;
  maybe& operator=(const maybe& other) = delete;

  ~maybe() {
    if(set) {
      get().~T();
    }
  }

  const T& get() const {
    assert(set);
    return *reinterpret_cast<const T*>(&storage);
  }
  
  T& get() {
    assert(set);
    return *reinterpret_cast<T*>(&storage);
  }
};

template<class T>
static maybe<T> some(const T& value) { return value; }

template<class A, class F>
static auto map(const maybe<A>& self, F f) {
  using result_type = decltype(f(self.get()));
  
  if(!self) {
    return maybe<result_type>{};
  }
  
  return some(f(self.get()));
}


#endif
