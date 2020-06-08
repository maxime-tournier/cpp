#ifndef PARSER_HPP
#define PARSER_HPP

#include "either.hpp"
#include <cassert>

namespace parser {

  struct range {
    const char* const first;
    const char* const last;
    
    explicit operator bool() const {
      return first != last;
    }

    char get() const {
      assert(bool(*this));      
      return *first;
    }
    
    range next() const {
      assert(bool(*this));
      return {first + 1, last};
    }
    
  };

  template<class T>
  struct success {
    const T value;
    const range rest;
  };

  template<class T>
  using result = either<range, success<T>>;

  template<int (*pred) (int)>
  static result<char> chr(range in) {
    if(!in) return in;
    if(!pred(in.get())) return in;
    return success<char>{in.get(), in.next()};
  };

  template<class Parser>
  using value_type = typename std::result_of<Parser(range)>::type;
  
  
}

#endif

