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
      integer push_var() { return size++; }
      
      // push named (unique) local variable
      integer push_var(symbol s) {
        auto res = locals.insert( std::make_pair(s, push_var() ) );
        if(!res.second) {
          throw slip::error("duplicate variable");
        }
        
        return res.first->second;
      }

      // void pop_var(std::size_t n = 1) { size -= n; }
      
      // declare anonymous/named function args, return its index (backwards) from stack top
      integer push_arg() { return args++; }
      integer push_arg(symbol s) {
        auto res = locals.insert( std::make_pair(s, -push_arg() ) );
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
