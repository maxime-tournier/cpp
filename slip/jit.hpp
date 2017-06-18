#ifndef LISP_JIT_HPP
#define LISP_JIT_HPP

#include "vm.hpp"
#include "codegen.hpp"

namespace slip {

  class jit {
    vm::machine machine;
    vm::bytecode code;
    ref<codegen::variables> ctx;
  public:
    
    jit();
    ~jit();

    // push named values on the stack
    jit& def(symbol name, const vm::value& value);
    
    vm::value eval(const slip::sexpr& expr, bool dump = false);

    // note: arguments will be moved
    vm::value call(const vm::value& func,
                   const vm::value* first, const vm::value* last);
    
  };

  

}


#endif
