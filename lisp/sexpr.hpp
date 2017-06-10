#ifndef LISP_SEXPR_HPP
#define LISP_SEXPR_HPP

#include "../variant.hpp"

#include "list.hpp"
#include "symbol.hpp"

#include <set>

namespace lisp {


  struct sexpr;

  using string = std::string;

  using boolean = bool;
  using integer = long;
  using real = double; 
  
 

  struct sexpr : variant< list<sexpr>,
                          boolean,
                          integer, 
                          real,
                          symbol,
                          ref<string> > {

    using list = lisp::list<sexpr>;
    
    using sexpr::variant::variant;
    
    explicit operator bool() const {
      return !is<list>() || get<list>();
    }
    
  };
  
  std::ostream& operator<<(std::ostream& out, const sexpr& self);  




  
}


#endif
