#ifndef MAYBE_HPP
#define MAYBE_HPP

#include "variant.hpp"

struct none { };

template<class T>
struct maybe : public variant<none, T> {
  using variant<none, T>::variant;

  explicit operator bool() const {
    return this->type();
  }
  
};



#endif
