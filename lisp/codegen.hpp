#ifndef LISP_CODEGEN_HPP
#define LISP_CODEGEN_HPP

#include <map>

#include "eval.hpp"
#include "context.hpp"

namespace lisp {

  namespace vm {
    class bytecode;
  }

  namespace codegen {

    
    struct variables : lisp::context<variables, std::size_t> {

      using context::context;
      
      locals_type captured;

      const symbol* defining = nullptr;

      integer add_local(symbol s) {
        auto res = locals.insert( std::make_pair(s, locals.size()));
        if(!res.second) throw lisp::error("duplicate local");
        return res.first->second;
      }

      integer capture(symbol s) {
        if(!parent) throw lisp::unbound_variable(s);
      
        auto res = captured.insert( std::make_pair(s, captured.size()));
        return res.first->second;
      }
    
    };

  }
  
  void compile(vm::bytecode& res, ref<codegen::variables>& ctx, const lisp::sexpr& e);
  

}


#endif
