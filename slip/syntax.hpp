#ifndef LISP_SYNTAX_HPP
#define LISP_SYNTAX_HPP


#warning use ast.hpp instead

// TODO this is weird
#include "eval.hpp"

namespace slip {
  
  // macro-expand + syntax check
  sexpr expand(const ref<environment>& ctx, const sexpr& expr);
  sexpr expand_seq(const ref<environment>& ctx, const sexpr& expr);
  sexpr expand_toplevel(const ref<environment>& ctx, const sexpr& expr);  
  
}



#endif
