#include "jit.hpp"

#include "codegen.hpp"

#include "ast.hpp"

#include <iostream>


namespace slip {

  jit::jit()
    : ctx( make_ref<codegen::variables>()) {

    // machine.stack.reserve( 2048 );
    
  }
  
  jit::~jit() { }


  jit& jit::def(symbol name, const vm::value& value) {
    ctx->push_var(name);
    machine.data_stack.emplace_back( value );
    return *this;
  }
  
  
  
  vm::value jit::eval(const ast::toplevel& node, bool dump) {
    const std::size_t start = code.size();
    
    codegen::compile(code, ctx, node);
    code.push_back( vm::opcode::HALT );
    
    if( dump ) {
      std::cout << "bytecode:" << std::endl;
      code.dump(std::cout, start);
    }
    
    code.link(start);

    const std::size_t stack_size = machine.data_stack.size();
    
    machine.run(code, start);
    const vm::value res = std::move(machine.data_stack.back());
    machine.data_stack.pop_back();

    if( dump ) {
      std::cout << "stack:" << std::endl;      
      std::cout << machine << std::endl;
    }

    if( machine.data_stack.size() == stack_size ) {
      // code chunk was expression evaluation without binding: clear
      code.resize(start, vm::opcode::NOOP);
    }
    
    
    return res;
  }



  vm::value jit::call(const vm::value& func, const vm::value* first, const vm::value* last) {
    throw error("broken lol");
    
    // const std::size_t init_stack_size = machine.data_stack.size(); (void) init_stack_size;

    // // TODO switch on func type
    // switch( func.type() ) {
    // case vm::value::type_index< ref<vm::closure> >(): {
    //   // call stubs
    //   using stub_type = std::map<const vm::closure*, std::size_t>;
    //   static stub_type stub;
    
    //   // push callable on the stack
    //   machine.data_stack.emplace_back(func);
    
    //   // push args on the stack
    //   for(const vm::value* it = first; it != last; ++it) {
    //     machine.data_stack.emplace_back( std::move(*it) );
    //   }

    //   const std::size_t argc = last - first;

    //   // note: we don't want key to prevent gc here
    //   const vm::closure* key = func.get<ref<vm::closure>>().get();
    //   const std::size_t start = code.size();
    
    //   auto err = stub.insert( std::make_pair(key, start) );
    //   if(err.second) {
    //     // create call stub
    //     code.emplace_back( vm::opcode::CALL );
    //     code.emplace_back( vm::integer(argc) );      
    //     code.emplace_back( vm::opcode::HALT );            
    //   }
    
    //   machine.run(code, err.first->second);
    //   vm::value result = machine.data_stack.back();
    //   machine.data_stack.pop_back();
    //   assert(machine.data_stack.size() == init_stack_size);
    //   return result;
    // }
      
    // case vm::value::type_index< vm::builtin >(): {
    //   throw error("unimplemented");
    //   // return func.get<vm::builtin>()( first, last );
    // }

    // default:
    //   throw type_error("callable expected");
    // }
    
  }
   
  
}
