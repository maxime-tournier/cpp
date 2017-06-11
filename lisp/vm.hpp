#ifndef LISP_VM_HPP
#define LISP_VM_HPP

#include "eval.hpp"

#include "../dynamic_sized.hpp"

namespace lisp {
  namespace vm {

    struct value;
    struct closure;
  
    enum class opcode : lisp::integer {
      NOOP,
        STOP, 

        PUSH,
        POP,
        SWAP,
      
        LOAD,
        STORE,

        LOADC,
        STOREC,
      
        CALL,
        RET,
      
        CLOS,
      
        JNZ,
        JMP

        };



    struct closure;
    

    
    using lisp::unit;
    using lisp::boolean;    
    using lisp::integer;
    using lisp::symbol;
    using lisp::real;
    using lisp::list;
    using lisp::builtin;
    using lisp::string;
  
    struct label : symbol {

      // TODO use separate symbol table
      using symbol::symbol;
    
    };
  
  
  
    struct value : variant< unit, boolean, integer, real, symbol, ref<string>, list<value>,
                            builtin,
                            ref<closure>,
                            opcode,
                            label> {
      using list = lisp::list<value>;
      
      using value::variant::variant;

      value(const sexpr& other) : value::variant( reinterpret_cast<const variant&> (other)) { }
      value(sexpr&& other) : value::variant( reinterpret_cast<variant&&> (other)) { }    

      // make sure we get moves on std::vector realloc
      static_assert(std::is_nothrow_move_constructible<variant>::value, "derp");
      static_assert(std::is_nothrow_destructible<variant>::value, "derp");
    };


    struct closure_head {
      const std::size_t argc;
      const std::size_t addr;
    };
    
    struct closure : closure_head, dynamic_sized<value> {
      
      template<class Iterator>
      closure(std::size_t argc,
              std::size_t addr,
              Iterator first, Iterator last)
        : closure_head{argc, addr},
          closure::dynamic_sized(first, last)
      {
        
      }

      friend ref<closure> make_closure(std::size_t argc, std::size_t addr,
                                       const value* first, const value* last);
      
    };

    

    class bytecode : public std::vector<value> {
      std::map< vm::label, integer > labels;
    public:
    
      void label(vm::label s);
      void link(std::size_t start = 0);

      void dump(std::ostream& out, std::size_t start = 0) const;
    };

  
    static inline std::ostream& operator<<(std::ostream& out, const bytecode& self) {
      self.dump(out);
      return out;
    }
  
  
    static_assert(sizeof(value) == sizeof(lisp::value),
                  "value size mismatch");

  
  
    std::ostream& operator<<(std::ostream& out, const value& self);
  

  
    struct machine {
      machine();

      // frame pointer stack
      std::vector<std::size_t> fp;

      // data stack
      std::vector<value> stack;
      
      void run(const bytecode& code, std::size_t start = 0);
      
    };

    std::ostream& operator<<(std::ostream& out, const machine& self);
  


  }
}
#endif
