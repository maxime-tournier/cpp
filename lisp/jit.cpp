#include "jit.hpp"

#include "eval.hpp"
#include "codegen.hpp"

#include <iostream>


namespace lisp {

  jit::jit()
    : ctx( make_ref<codegen::context>()) {
    // TODO initialize stack size?
  }
  
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
      machine.stack.emplace_back( it.second.map<vm::value>(import_visitor()) );
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
    const vm::value res = std::move(machine.stack.back());
    machine.stack.pop_back();

    return res;
  }



  vm::value jit::call(const vm::value& func, const vm::value* first, const vm::value* last) {
    const std::size_t init_stack_size = machine.stack.size(); (void) init_stack_size;

    // TODO switch on func type

    if(!func.is<ref<vm::closure>>()) throw error("jit::call: not implemented");

    // call stubs
    using stub_type = std::map<const vm::closure*, std::size_t>;
    static stub_type stub;
    
    // push callable on the stack
    machine.stack.emplace_back(func);
    
    // push args on the stack
    for(const vm::value* it = first; it != last; ++it) {
      machine.stack.emplace_back( std::move(*it) );
    }

    const std::size_t argc = last - first;
    
    const vm::closure* key = func.get<ref<vm::closure>>().get();
    const std::size_t start = code.size();
    
    auto err = stub.insert( std::make_pair(key, start) );
    if(err.second) {
      // create call stub
      code.emplace_back( vm::opcode::CALL );
      code.emplace_back( vm::integer(argc) );      
      code.emplace_back( vm::opcode::STOP );            
    }
    
    machine.run(code, err.first->second);
    vm::value result = machine.stack.back();
    machine.stack.pop_back();
    assert(machine.stack.size() == init_stack_size);
    return result;
  }
   
  
}
