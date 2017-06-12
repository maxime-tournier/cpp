#include "jit.hpp"

#include "eval.hpp"
#include "codegen.hpp"

#include <iostream>


namespace lisp {

  jit::jit()
    : ctx( make_ref<codegen::context>()) {

    // machine.stack.reserve( 2048 );
    
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

    if( dump ) {
      std::cout << "bytecode:" << std::endl;
      code.dump(std::cout, start);
    }
    
    code.link(start);

    const std::size_t stack_size = machine.stack.size();
    
    machine.run(code, start);
    const vm::value res = std::move(machine.stack.back());
    machine.stack.pop_back();

    if( dump ) {
      std::cout << "stack:" << std::endl;      
      std::cout << machine << std::endl;
    }

    if( machine.stack.size() == stack_size ) {
      // code chunk was expression evaluation without binding: clear
      code.resize(start, lisp::unit());
    }
    
    
    return res;
  }



  vm::value jit::call(const vm::value& func, const vm::value* first, const vm::value* last) {
    const std::size_t init_stack_size = machine.stack.size(); (void) init_stack_size;

    // TODO switch on func type
    switch( func.type() ) {
    case vm::value::type_index< ref<vm::closure> >(): {
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

      // note: we don't want key to prevent gc here
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
    case vm::value::type_index< vm::builtin >(): {

      const vm::builtin& ptr = func.get< vm::builtin >();
      lisp::value result = ptr( reinterpret_cast<const lisp::value*>(first),
                                reinterpret_cast<const lisp::value*>(last) );
      
      return reinterpret_cast<vm::value&>(result);
    }

    default:
      throw type_error("callable expected");
    }
    
  }
   
  
}
