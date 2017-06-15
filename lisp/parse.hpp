#ifndef LISP_PARSE_HPP
#define LISP_PARSE_HPP

#include <iostream>
#include "../parse.hpp"

namespace lisp {

  struct sexpr;

  parse::any<sexpr> skipper();  
  parse::any<sexpr> parser();
  
}


#endif
