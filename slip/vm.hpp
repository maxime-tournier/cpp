#ifndef LISP_VM_HPP
#define LISP_VM_HPP

#include "eval.hpp"

#include "../dynamic_sized.hpp"

// #include <iostream>

namespace slip {

  class jit;
  
  namespace vm {

    struct value;
    struct closure;
  
    enum class opcode : slip::integer {
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

        RECORD,
        GETATTR,
        
        JNZ,
        JMP,

        PEEK,
        
        };



    struct closure;
    struct record;

    
    using slip::unit;
    using slip::boolean;    
    using slip::integer;
    using slip::symbol;
    using slip::real;
    using slip::list;

    using builtin = value (*)(const value* first, const value* last);
    
    using slip::string;

    struct label : symbol {

      // TODO use separate symbol table
      using symbol::symbol;
    
    };
  
  
    struct value : variant< unit, boolean, integer, real, symbol, ref<string>, list<value>,
                            builtin,
                            ref<closure>, ref<record>,
                            opcode,
                            label> {
      using list = slip::list<value>;
      
      using value::variant::variant;

      value(const sexpr& other) : value::variant( reinterpret_cast<const variant&> (other)) { }
      value(sexpr&& other) : value::variant( reinterpret_cast<variant&&> (other)) { }    

      // make sure we get moves on std::vector realloc
      static_assert(std::is_nothrow_move_constructible<variant>::value, "derp");
      static_assert(std::is_nothrow_destructible<variant>::value, "derp");
    };


    struct closure_head : detail::rc_base {
      const std::size_t argc;
      const std::size_t addr;

      closure_head(std::size_t argc, std::size_t addr)
        : argc(argc),
          addr(addr) {
        
      }
    };
    
    struct closure : closure_head, dynamic_sized<value> {
      

      // private:
      template<class Iterator>
      closure(std::size_t argc, // TODO do we actually need this?
              std::size_t addr,
              Iterator first, Iterator last)
        : closure_head{argc, addr},
          closure::dynamic_sized(first, last)
      {
        // std::clog << "closure created: " << addr << std::endl;
      }

    };

    ref<closure> make_closure(std::size_t argc, std::size_t addr,
                              const value* first, const value* last);

    

    struct record_head : detail::rc_base {
      std::size_t magic;
    };

    struct record : record_head, dynamic_sized<value> {
      using record::dynamic_sized::dynamic_sized;
    };
    
    ref<record> make_record(const value* first, const value* last);

    
    
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
  
  
    static_assert(sizeof(value) == sizeof(slip::value),
                  "value size mismatch");

  
  
    std::ostream& operator<<(std::ostream& out, const value& self);
  

  
    struct machine {
      machine();

      // frame pointer stack
      std::vector<std::size_t> fp;

      // data stack
      using stack_type = std::vector<value>;
      stack_type stack;
      
      void run(const bytecode& code, std::size_t start = 0);

    };

    std::ostream& operator<<(std::ostream& out, const machine& self);
  


  }
}
#endif
