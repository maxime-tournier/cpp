#include "jit.hpp"

#include "eval.hpp"
#include "codegen.hpp"

#include <iostream>


namespace lisp {

  jit::jit()
    : ctx( make_ref<codegen::context>()) { }
  
  jit::~jit() { }


  struct import_visitor {

    vm::value operator()(const sexpr& self) const {
      return self;
    }

    vm::value operator()(const builtin& self) const {
      return self;
    }


    template<class T>
    vm::value operator()(const T& self) const {
      throw error("cannot import values of this type");
    }
    
  };


  
  
  void jit::import(const ref<lisp::context>& env) {

    for(const auto& it : env->locals) {
      ctx->add_local(it.first);
      machine.data.push_back( it.second.map<vm::value>(import_visitor()) );
    }
    
  }
  
  vm::value jit::eval(const lisp::sexpr& expr, bool dump) {
    const std::size_t start = code.size();

    compile(code, ctx, expr);
    code.push_back( vm::opcode::STOP );

    if(dump) {
      std::cout << "bytecode:" << std::endl;
      code.dump(std::cout, start);
    }
    
    code.link(start);

    machine.run(code, start);
    const vm::value res = std::move(machine.data.back());
    machine.data.pop_back();
    
    return res;
  }


}
