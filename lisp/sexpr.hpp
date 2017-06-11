#ifndef LISP_SEXPR_HPP
#define LISP_SEXPR_HPP

#include "../variant.hpp"

#include "list.hpp"
#include "symbol.hpp"

#include <set>

namespace lisp {


  struct sexpr;

  using string = std::string;

  struct unit {};
  using boolean = bool;
  using integer = long;
  using real = double; 
  

  struct sexpr : variant< unit,
                          boolean,
                          integer, 
                          real,
                          symbol,
                          ref<string>,
                          list<sexpr> > {

    using list = lisp::list<sexpr>;
    
    using sexpr::variant::variant;
    
  };
  
  std::ostream& operator<<(std::ostream& out, const sexpr& self);  




  
}


#endif
