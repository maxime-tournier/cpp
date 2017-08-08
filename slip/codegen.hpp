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

    
    struct variables : slip::context<variables, integer> {
      std::size_t size = 0, args = 1;
      
      using context::context;

      using captured_type = std::map<symbol, std::size_t>;
      captured_type captured;
      
      const symbol* defining = nullptr;

      // push unnamed local variable, return its index
      integer add_var() { return size++; }

      // push named (unique) local variable
      integer add_var(symbol s) {
        auto res = locals.insert( std::make_pair(s, add_var() ) );
        if(!res.second) {
          throw slip::error("duplicate variable");
        }
        
        return res.first->second;
      }

      integer add_arg() { return args++; }
      integer add_arg(symbol s) {
        auto res = locals.insert( std::make_pair(s, -add_arg() ) );
        if(!res.second) {
          throw slip::error("duplicate argument");
        }
        return res.first->second;
      }
      
      

      std::size_t capture(symbol s) {
        if(!parent) throw slip::unbound_variable(s);
      
        auto res = captured.insert( std::make_pair(s, captured.size()));
        return res.first->second;
      }
    
    };


    void compile(vm::bytecode& res, ref<variables>& ctx, const ast::toplevel& e);
  }
}


#endif
