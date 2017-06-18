#ifndef LISP_PARSE_HPP
#define LISP_PARSE_HPP

#include <iostream>
#include "../parse.hpp"

namespace slip {

  struct sexpr;

  parse::any<sexpr> skipper();  
  parse::any<sexpr> parser();
  
}


#endif
