#ifndef LISP_VM_HPP
#define LISP_VM_HPP

#include "eval.hpp"

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



  
    struct closure {
      std::vector<value> capture;
      std::size_t argc;
      std::size_t addr;
    };

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
  
  
  
    struct value : variant< list<value> ,
                            integer, 
                            real,
                            symbol,
                            ref<string>,
                            builtin,
                            ref<closure>,
                            opcode,
                            label> {
      using list = lisp::list<value>;
      
      using value::variant::variant;

      inline explicit operator bool() const {
        return !is<list>() || get<list>();
      }

      value(const sexpr& other) : value::variant( reinterpret_cast<const variant&> (other)) { }
      value(sexpr&& other) : value::variant( reinterpret_cast<variant&&> (other)) { }    
      
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
    
      std::vector<std::size_t> fp;
      std::vector<value> data;

      void run(const bytecode& code, std::size_t start = 0);
    


    };

    std::ostream& operator<<(std::ostream& out, const machine& self);
  


  }
}
#endif
