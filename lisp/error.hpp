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
  
}

#endif
