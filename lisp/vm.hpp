#ifndef VM_HPP
#define VM_HPP

#include "value.hpp"

namespace vm {

  struct value;
  struct closure;
  
  class enum opcode : lisp::integer {
    NOOP = 0,
      PUSH,      
      LOAD,
      STORE,
      
      CALL,
      
      RET,
      CLOSE,
      
      JNE,
      JMP
      };
  
  struct value : variant< lisp::list,
                          lisp::integer,
                          lisp::real,
                          lisp::symbol,
                          ref<lisp::string>,
                          lisp::builtin,
                          closure,
                          opcode> {
    
  };

  

  struct closure {
    std::vector<value> captured;
    bytecode code;
  };
  
  using bytecode = std::vector<value>;
  
  struct machine {

    std::vector<std::size_t> fp;
    std::vector<value> data;
    
    void run(const bytecode& code) {

      const value* op = code.data();
      fp.push(
      while(true) {

        assert(op->is<opcode>());

        switch(op->get<opcode>()) {

        case NOOP: break;
        case PUSH: data.push_back(*++op); break;
        case POP: data.pop_back(); break;
        case LOAD: 
          ++op; assert( op->is<lisp::integer>());
          data.push_back( data[fp.top() + op->get<lisp::integer>()]);
          break;
        case STORE: 
          ++op; assert( op->is<lisp::integer>());
          auto& dst = data[fp.top() + op->get<lisp::integer>()];
          dst = *++op;
          break;
        case CALL: {
          ++op; assert( op->is<lisp::integer>());
          const lisp::integer n = op->get<lisp::integer>();
          assert(n <= data.size());
          
          ++op; assert( op->is<lisp::builtin>());
          const lisp::builtin func = op->get<lisp::builtin>();
          const lisp::value res = func( &data.end()[-n], &data.end()[0] );
          data.resize( data.size() - n );
          // TODO reinterpret_cast
          data.push_back( reinterpret_cast<const value&>(val) );
          break;
        }
        };
        
        ++op;
      };

      
    }
    

  };
  

}

#endif
