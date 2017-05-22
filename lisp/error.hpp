#ifndef ERROR_HPP
#define ERROR_HPP

#include <stdexcept>

namespace lisp {

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  
}

#endif
