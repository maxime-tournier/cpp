#ifndef LISP_CODEGEN_HPP
#define LISP_CODEGEN_HPP

#include <map>
#include "eval.hpp"


namespace lisp {

  namespace vm {
    struct bytecode;
  }

  namespace codegen {

    struct context {
      ref<context> parent;

      context(const ref<context>& parent = {})
        : parent(parent) { }

      using locals_type = std::map<symbol, integer>;
      locals_type locals;
      std::map< symbol, integer > captures;    

      const symbol* defining = nullptr;
    
      integer add_local(symbol s) {
        auto res = locals.insert( std::make_pair(s, locals.size()));
        if(!res.second) throw lisp::error("duplicate local");
        return res.first->second;
      }

      integer capture(symbol s) {
        if(!parent) throw lisp::unbound_variable(s);
      
        auto res = captures.insert( std::make_pair(s, captures.size()));
        return res.first->second;
      }
    
    };

  }
  
  void compile(vm::bytecode& res, ref<codegen::context>& ctx, const lisp::sexpr& e);
  

}


#endif
