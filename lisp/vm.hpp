#ifndef VM_HPP
#define VM_HPP

#include "value.hpp"
#include "eval.hpp"

namespace vm {

  struct value;
  struct closure;
  
  enum class opcode : lisp::integer {
    NOOP,
      STOP, 

      PUSH,
      POP,
      LOAD,
      STORE,

      CALL,
      RET,
      
      CLOSE,
      
      JNE,
      JMP
      };



  
  struct closure {
    std::vector<value> capture;
    std::size_t addr;
  };

  struct label : lisp::symbol {

    // TODO use separate symbol table
    using lisp::symbol::symbol;
    
  };
  
  
  
  struct value : variant< lisp::list,
                          lisp::integer, 
                          lisp::real,
                          lisp::symbol,
                          ref<lisp::string>,
                          lisp::builtin,
                          ref<closure>,
                          opcode,
                          label> {
    using value::variant::variant;

    struct ostream;
  };


  class bytecode : public std::vector<value> {
    std::map< vm::label, lisp::integer > labels;
  public:
    
    void label(vm::label s);
    void link();
    
  };
  
  static_assert(sizeof(value) == sizeof(lisp::value),
                "value size mismatch");

  
  
  std::ostream& operator<<(std::ostream& out, const value& self);
  

  
  struct machine {
    
    std::vector<std::size_t> fp;
    std::vector<value> data;

    void run(const bytecode& code);
    

  };
  

}

#endif
