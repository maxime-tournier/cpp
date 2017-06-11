#ifndef LISP_ERROR_HPP
#define LISP_ERROR_HPP

#include <stdexcept>

namespace lisp {

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  struct type_error : error {
    using error::error;
  };

  struct argument_error : error {
    argument_error(std::size_t got, std::size_t expected)
      : error("bad argument count: " + std::to_string(got) + " " + std::to_string(expected) + " <= expected"){
      
    }
    
  };

  
}

#endif
