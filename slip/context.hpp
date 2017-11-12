#ifndef LISP_CONTEXT_HPP
#define LISP_CONTEXT_HPP

#include <map>

#include "symbol.hpp"
#include "../ref.hpp"


namespace slip {

  template<class Derived, class Value>
  class context {
  protected:
    using base = context;
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

    const value_type* find_local(symbol key) const {
      auto it = locals.find(key);
      if(it != locals.end()) return &it->second;
      return nullptr;
    }
    
    const value_type* find(symbol key) const {
      const value_type* res = find_local(key);
      if( res ) return res;
      if( parent ) return parent->find(key);
      return nullptr;
    }


    std::size_t debug(std::ostream& out) const {
      std::size_t indent = 0;
      if(parent) indent = 1 + parent->debug(out);

      for(const auto& p: locals) {
        for(std::size_t i = 0; i < indent; ++i) out << " ";
        out << p.first << ": " << p.second << '\n';
      }
      
      return indent;
    }
    
  };

}



#endif
