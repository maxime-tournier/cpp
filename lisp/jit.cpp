#include "jit.hpp"

#include "eval.hpp"
#include "codegen.hpp"

namespace lisp {

  jit::jit()
    : ctx( make_ref<codegen::context>()),
      env( make_ref<lisp::context>()) { }
  
  jit::~jit() { }

  void jit::import(const ref<lisp::context>& env) {

    this->env->locals.insert(env->locals.begin(),
                             env->locals.end());
    
    for(const auto& it : env->locals) {
      eval( symbol("def") >>= it.first >>= it.second >>= lisp::nil );
    }

  }
  
  vm::value jit::eval(const lisp::value& expr) {
    const std::size_t start = code.size();

    compile(code, ctx, expr);
    code.push_back( vm::opcode::STOP );

    // std::cout << "bytecode:" << std::endl;
    // code.dump(std::cout, start);
    
    code.link(start);

    m.run(code, start);
    const vm::value res = std::move(m.data.back());
    m.data.pop_back();
    
    return res;
  }


}
