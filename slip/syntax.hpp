#ifndef LISP_SYNTAX_HPP
#define LISP_SYNTAX_HPP


#warning use ast.hpp instead

// TODO this is weird
#include "eval.hpp"

namespace slip {
  
  struct syntax_error : error {
    using error::error;
  };
  
  // macro-expand + syntax check
  sexpr expand(const ref<environment>& ctx, const sexpr& expr);
  sexpr expand_seq(const ref<environment>& ctx, const sexpr& expr);
  sexpr expand_toplevel(const ref<environment>& ctx, const sexpr& expr);  
  
  namespace kw {
    const extern symbol def, lambda, cond, wildcard;
    const extern symbol seq, var, pure, run;
    
    const extern symbol record;
    
    const extern symbol quote, unquote, quasiquote;
    
    const extern symbol type;

    const extern symbol ref, get, set;
    
    // const extern symbol ref, getref, setref;
  }
  
}



#endif
