#ifndef LISP_ERROR_HPP
#define LISP_ERROR_HPP

#include <stdexcept>
#include <string>

namespace slip {

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };


  struct parse_error : error {
    using error::error;
  };

  struct type_error : error {
    using error::error;
  };

  struct kind_error : error {
    using error::error;
  };

  
  struct argument_error : error {
    argument_error(std::size_t got, std::size_t expected)
      : error("bad argument count: " + std::to_string(got) + ", expected: " + std::to_string(expected)) {
      
    }

    static void check(std::size_t got, std::size_t expected) {
      if(got != expected) throw argument_error(got, expected);
    }
  };

  
}

#endif
