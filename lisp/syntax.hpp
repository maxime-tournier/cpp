#ifndef LISP_SYNTAX_HPP
#define LISP_SYNTAX_HPP

#include "value.hpp"

namespace lisp {

  
  struct syntax_error : error {
    using error::error;
  };
  
  // macro-expand + syntax check
  value expand(const ref<context>& ctx, const value& expr);
  value expand_seq(const ref<context>& ctx, const value& expr);

  namespace kw {
    const extern symbol def, lambda, seq, cond, quote;
  }
  
}



#endif
