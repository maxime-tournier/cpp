#ifndef LISP_JIT_HPP
#define LISP_JIT_HPP

#include "vm.hpp"
#include "codegen.hpp"

namespace lisp {

  class jit {
    vm::machine machine;
    vm::bytecode code;
    ref<codegen::variables> ctx;
  public:
    
    jit();
    ~jit();

    // not sure though
    void import(const ref<lisp::environment>& ctx);
    
    vm::value eval(const lisp::sexpr& expr, bool dump = false);

    // note: arguments will be moved
    vm::value call(const vm::value& func,
                   const vm::value* first, const vm::value* last);
    
  };

  

}


#endif
