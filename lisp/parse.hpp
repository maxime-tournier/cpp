#ifndef LISP_PARSE_HPP
#define LISP_PARSE_HPP

#include <iostream>
#include "../parse.hpp"

namespace lisp {

  struct value;
  parse::any<value> parser();
  
}


#endif
