#ifndef LISP_JIT_HPP
#define LISP_JIT_HPP

#include "vm.hpp"
#include "codegen.hpp"

namespace lisp {

  class jit {
    vm::machine machine;
    vm::bytecode code;
    ref<codegen::context> ctx;
  public:
    
    jit();
    ~jit();

    // not sure though
    void import(const ref<lisp::context>& ctx);
    
    vm::value eval(const lisp::sexpr& expr);
  };

  

}


#endif
