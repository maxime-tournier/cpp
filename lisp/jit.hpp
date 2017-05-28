#ifndef LISP_JIT_HPP
#define LISP_JIT_HPP

#include "vm.hpp"
#include "codegen.hpp"

namespace lisp {

  class jit {
    vm::machine m;
    vm::bytecode code;
    ref<codegen::context> ctx;
    ref<lisp::context> env;
  public:
    
    jit();
    ~jit();

    // not sure though
    void import(const ref<lisp::context>& ctx);
    
    vm::value eval(const lisp::value& expr);
  };

  

}


#endif
