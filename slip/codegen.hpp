#ifndef LISP_CODEGEN_HPP
#define LISP_CODEGEN_HPP

#include <map>

#include "eval.hpp"
#include "context.hpp"

namespace slip {

  namespace vm {
    class bytecode;
  }

  namespace ast {
    struct toplevel;
  };

  namespace codegen {

    
    struct variables : slip::context<variables, std::size_t> {

      using context::context;
      
      locals_type captured;

      const symbol* defining = nullptr;

      integer add_local(symbol s) {
        auto res = locals.insert( std::make_pair(s, locals.size()));
        if(!res.second) throw slip::error("duplicate local");
        return res.first->second;
      }

      integer capture(symbol s) {
        if(!parent) throw slip::unbound_variable(s);
      
        auto res = captured.insert( std::make_pair(s, captured.size()));
        return res.first->second;
      }
    
    };

  }



  
  void compile(vm::bytecode& res, ref<codegen::variables>& ctx, const slip::sexpr& e);


  namespace codegen {
    void compile(vm::bytecode& res, ref<variables>& ctx, const ast::toplevel& e);
  }
}


#endif
