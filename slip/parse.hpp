#ifndef LISP_PARSE_HPP
#define LISP_PARSE_HPP

#include <iostream>
#include "../parse.hpp"

namespace slip {

  struct sexpr;
  struct unit;
  
  parse::any<unit> skipper();  
  parse::any<sexpr> parser();
  
}


#endif
