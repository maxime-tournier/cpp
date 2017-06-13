#ifndef LISP_CONTEXT_HPP
#define LISP_CONTEXT_HPP

#include "symbol.hpp"
#include "../ref.hpp"

namespace lisp {

  template<class Value>
  class context {
  protected:
    ref<context> parent;
  public:
    
    using value_type = Value;
    
    using locals_type = std::map<symbol, value_type>;
    locals_type locals;
    
    context(const ref<context>& parent = {}) 
      : parent(parent) { }
    
    value_type* find(symbol key) {
      auto it = data.find(key);
      
      if(it != data.end()) return &it->second;
      if(parent) return parent->find(key);
      return nullptr;
    }

    
  };

}



#endif
