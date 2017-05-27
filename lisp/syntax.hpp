#ifndef LISP_SYNTAX_HPP
#define LISP_SYNTAX_HPP

#include "value.hpp"

namespace lisp {

  
  struct syntax_error : error {
    syntax_error(const std::string& s);
  };
  
  // macro-expand + syntax check
  value expand(const ref<context>& ctx, const value& expr);

}



#endif
