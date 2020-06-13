#ifndef FINALLY_HPP
#define FINALLY_HPP

#include <utility>

template<class Func>
static auto finally(Func func) {
  struct lock {
    Func func;
    ~lock() {
      func();
    }
  };

  return lock{std::move(func)};
}

#endif
