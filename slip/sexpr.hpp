#ifndef LISP_SEXPR_HPP
#define LISP_SEXPR_HPP

#include "variant.hpp"

#include "list.hpp"
#include "symbol.hpp"

#include <set>

namespace slip {

  struct sexpr;

  using string = std::string;

  struct unit {
    inline bool operator==(const unit&) const { return true; }
  };
  
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

    using list = slip::list<sexpr>;
    
    using sexpr::variant::variant;
    
  };
  
  std::ostream& operator<<(std::ostream& out, const sexpr& self);  




  
}


#endif
