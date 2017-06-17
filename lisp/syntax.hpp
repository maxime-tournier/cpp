#ifndef LISP_SYNTAX_HPP
#define LISP_SYNTAX_HPP

// TODO this is weird
#include "eval.hpp"

namespace lisp {
  
  struct syntax_error : error {
    using error::error;
  };
  
  // macro-expand + syntax check
  sexpr expand(const ref<environment>& ctx, const sexpr& expr);
  sexpr expand_seq(const ref<environment>& ctx, const sexpr& expr);
  
  namespace kw {
    const extern symbol def, lambda, seq, cond, quote, unquote, quasiquote;

    const extern symbol var, set, pure, run;
    
    // const extern symbol ref, getref, setref;
  }
  
}



#endif
