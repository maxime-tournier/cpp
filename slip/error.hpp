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

  struct syntax_error : error {
    using error::error;
  };
  
  struct type_error : error {
    using error::error;
  };

  struct kind_error : error {
    using error::error;
  };

  
}

#endif
