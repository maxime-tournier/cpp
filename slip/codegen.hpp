#ifndef LISP_CODEGEN_HPP
#define LISP_CODEGEN_HPP

#include <map>

#include "sexpr.hpp"
#include "context.hpp"

namespace slip {

  namespace vm {
    class bytecode;
  }

  namespace ast {
    struct toplevel;
  };

  namespace codegen {

    
    struct variables : slip::context<variables, integer> {
      std::size_t size = 0, args = 1;
      
      using context::context;

      using captured_type = std::map<symbol, std::size_t>;
      captured_type captured;
      
      const symbol* defining = nullptr;

      // push anonymous local variable, return its index
      integer add_var() { return size++; }
      
      // push named (unique) local variable
      integer add_var(symbol s) {
        auto res = locals.insert( std::make_pair(s, add_var() ) );
        if(!res.second) {
          throw slip::error("duplicate variable");
        }
        
        return res.first->second;
      }

      // declare anonymous/named function args, return its index (backwards) from stack top
      integer add_arg() { return args++; }
      integer add_arg(symbol s) {
        auto res = locals.insert( std::make_pair(s, -add_arg() ) );
        if(!res.second) throw std::runtime_error("duplicate variable");
        
        return res.first->second;
      }
      

      // add s to the list of captured variables
      std::size_t capture(symbol s);
    
    };


    void compile(vm::bytecode& res, ref<variables>& ctx, const ast::toplevel& e);
  }
}


#endif
