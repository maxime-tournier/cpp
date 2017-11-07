#ifndef LISP_VM_HPP
#define LISP_VM_HPP

#include "eval.hpp"

#include "../dynamic_sized.hpp"

#include "../indices.hpp"

#include <iostream>

namespace slip {

  class jit;
  
  namespace vm {

    struct value;
    struct closure;


    // abstract data stack api for builtin functions
    struct stack;
    value pop(stack* );

    using builtin = value (*)(stack*);
    
    struct closure;

    struct record;
    struct partial;
    
    using slip::unit;
    using slip::boolean;    
    using slip::integer;
    using slip::symbol;
    using slip::real;
    using slip::list;

    using slip::string;

    // TODO proper nan-tagged values
    
    struct value : variant< unit, boolean, integer, real, symbol, ref<string>, list<value>,
                            builtin,
                            ref<closure>, ref<partial>,
                            ref<record> > {
      using list = slip::list<value>;
      
      using value::variant::variant;

      value(const sexpr& other) : value::variant( reinterpret_cast<const variant&> (other)) { }
      value(sexpr&& other) : value::variant( reinterpret_cast<variant&&> (other)) { }    

      // make sure we get moves on std::vector realloc
      static_assert(std::is_nothrow_move_constructible<variant>::value, "derp");
      static_assert(std::is_nothrow_destructible<variant>::value, "derp");

      
    };
    // not sure about this though
    static_assert(sizeof(value) == sizeof(slip::value), "value size mismatch");

    
    std::ostream& operator<<(std::ostream& out, const value& self);
  
    


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

    
    // partial application
    // TODO use records instead?
    struct partial : detail::rc_base, dynamic_sized<value> {
      using partial::dynamic_sized::dynamic_sized;
    };

    // simply a stack slice
    ref<partial> make_partial(const value* first, const value* last);

    
    struct record_head : detail::rc_base {
      std::size_t magic;
      record_head(std::size_t magic) : magic(magic) { }
    };

    struct record : record_head, dynamic_sized<value> {
      using record::dynamic_sized::dynamic_sized;

      record(std::size_t magic,
             const value* first,
             const value* last):
        record_head(magic),
        record::dynamic_sized(first, last) {

      }
    };
    
    ref<record> make_record(std::size_t magic, const value* first, const value* last);
    

    // bytecode
    enum class opcode {
      NOOP,
      HALT, 
      
      // PUSH,

      // TODO remove these ?
      PUSHU,                    
      PUSHB,
      PUSHI,
      PUSHR,
      PUSHS,
      
      POP,
      SWAP,
      
      LOAD,
      STORE,

      LOADC,
      STOREC,
      
      CALL,
      RET,

      JNZ,
      JMP,

      CLOS,

      RECORD,
      GETATTR,
        
      PEEK,

      OPCODE_COUNT
    };

    
    union instruction {
      instruction(opcode op) : op(op) { }
      explicit instruction(std::size_t value) : value(value) { }      
      
      explicit instruction(symbol label) : label(label) { }
      explicit instruction(integer i) : as_integer(i) { }
      explicit instruction(real r) : as_real(r) { }
      explicit instruction(bool b) : as_boolean(b) { }
      explicit instruction(const char* s) : as_string(s) { }      
      
      opcode op;
      std::size_t value;
      
      symbol label;
      
      integer as_integer;
      real as_real;
      bool as_boolean;
      const char* as_string;
    };

    static_assert(sizeof(instruction) == sizeof(void*), "instruction size error");
    
    // TODO you can only ever push to it, this should be more like a stream
    class bytecode : public std::vector<instruction> {
      std::map< symbol, std::size_t > labels;
    public:
    
      void label(symbol s);
      void link(std::size_t start = 0);

      void dump(std::ostream& out, std::size_t start = 0) const;
    };

  
    static inline std::ostream& operator<<(std::ostream& out, const bytecode& self) {
      self.dump(out);
      return out;
    }
  
  

  
    struct machine {
      machine();

      // call stack
      struct frame {
        // frame pointer
        const std::size_t fp;

        // return address
        const std::size_t addr;

        // frame current + remaining argument count (over-saturated calls)
        const std::size_t argc, cont;
      };

      using call_stack_type = std::vector<frame>;
      call_stack_type call_stack;
      
      // data stack
      using data_stack_type = std::vector<value>;
      data_stack_type data_stack;
      
      void run(const bytecode& code, std::size_t start = 0);

    };

    std::ostream& operator<<(std::ostream& out, const machine& self);




    // a few helpers
    template<class F, class Ret, class ... Args, std::size_t ... I>
    static value wrap(const F& f, Ret (*)(Args... args), indices<I...> indices) {
      static Ret(*ptr)(Args...) = f;
      
      return +[](vm::stack* args) -> value {
        vm::value expand[] = { ((void)I, pop(args))...};
        return ptr( std::move(expand[I].template get<Args>()) ... );
      };
      
    } 
 
    template<class F, class Ret, class ... Args>
    static value wrap(const F& f, Ret (*ptr)(Args... args)) {
      return wrap(f, ptr, type_indices<Args...>() );
    }
    
    template<class F>
    static value wrap(const F& f) {
      return wrap(f, +f);
    }
    


  }
}
#endif
