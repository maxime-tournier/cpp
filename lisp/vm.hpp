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

      LOADC,
      STOREC,
      
      CALL,
      RET,
      
      CLOS,
      
      JNE,
      JMP
      };



  
  struct closure {
    std::vector<value> capture;
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
  
  
  
  struct value : variant< list,
                          integer, 
                          real,
                          symbol,
                          ref<string>,
                          builtin,
                          ref<closure>,
                          opcode,
                          label> {
    using value::variant::variant;

    struct ostream;
  };


  class bytecode : public std::vector<value> {
    std::map< vm::label, integer > labels;
  public:
    
    void label(vm::label s);
    void link();

    friend std::ostream& operator<<(std::ostream& out, const bytecode& self);    
  };


  
  
  static_assert(sizeof(value) == sizeof(lisp::value),
                "value size mismatch");

  
  
  std::ostream& operator<<(std::ostream& out, const value& self);
  

  
  struct machine {
    machine();
    
    std::vector<std::size_t> fp;
    std::vector<value> data;

    void run(const bytecode& code);
    

  };
  

}

#endif
