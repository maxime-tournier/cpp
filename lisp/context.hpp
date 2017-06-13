#ifndef LISP_CONTEXT_HPP
#define LISP_CONTEXT_HPP

#include "symbol.hpp"
#include "../ref.hpp"

namespace lisp {

  template<class Derived, class Value>
  class context {
  protected:
    ref<Derived> parent;
  public:
    
    using value_type = Value;
    
    using locals_type = std::map<symbol, value_type>;
    locals_type locals;
    
    context(const ref<Derived>& parent = {}) 
      : parent(parent) {
      context* check = (Derived*) nullptr;
      (void) check;
    }
    
    value_type* find(symbol key) {
      auto it = locals.find(key);
      
      if(it != locals.end()) return &it->second;
      if(parent) return parent->find(key);
      return nullptr;
    }

    
  };

}



#endif
