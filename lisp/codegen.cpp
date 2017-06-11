#include "codegen.hpp"
#include "vm.hpp"

namespace lisp {

  // codegen
  namespace codegen {

    using vm::bytecode;
    using vm::label;
    using vm::opcode;


    using special = void (*)(bytecode& res, ref<context>&, const sexpr::list&);

  

    static void def(bytecode& res, ref<context>& ctx, const sexpr::list& args) {
      sexpr::list curr = args;
      const symbol name = curr->head.get<symbol>();
      ctx->add_local(name);
      
      curr = curr->tail;
      const sexpr& expr = curr->head;
      
      // TODO better ?
      ctx->defining = &name;
      compile(res, ctx, expr);
      ctx->defining = nullptr;
      
      // def evals to nil
      res.push_back( opcode::PUSH );
      res.push_back( unit() );
    };

  
    static integer unique() {
      static integer res = 0;
      return res++;
    }


    static void cond(bytecode& res, ref<context>& ctx, const sexpr::list& args) {
      const integer id = unique();

      const label start = "cond-" + std::to_string(id) + "-start";
      
      res.label(start);
      const label end = "cond-" + std::to_string(id) + "-end";
      
      std::map<label, sexpr> branch;

      // compile tests
      integer i = 0;
      for(const sexpr& item : args) {
        
        const sexpr::list& chunk = item.get<sexpr::list>(); 
        
        const sexpr& test = chunk->head;
        const sexpr& result = chunk->tail->head;
        
        const label then = "cond-" + std::to_string(id) + "-then-" + std::to_string(i);
        
        branch.emplace( std::make_pair(then, result) );

        // optimize literal boolean tests
        if(test.is<boolean>()) {
          
          if(test.get<boolean>()) {
            res.push_back( opcode::JMP );
            res.push_back( then );
          }
          
        } else {
          compile(res, ctx, test);
          res.push_back( opcode::JNZ );
          res.push_back( then );
        }
        ++i;
      }

      // don't forget to jump
      res.push_back( opcode::JMP );
      res.push_back( end );      

      // compile branches
      // TODO order branches ?
      for(const auto it : branch) {
        res.label(it.first);
        compile(res, ctx, it.second);
        res.push_back( opcode::JMP );
        res.push_back( end );      
      }

      res.label(end);
    }

    template<class T>
    static void literal(bytecode& res, const T& value) {
      res.push_back( opcode::PUSH );
      res.push_back( value );
    }

   
  

    static void quote(bytecode& res, ref<context>& ctx, const sexpr::list& args) {
      literal(res, args->head);
    }

    static void seq(bytecode& res, ref<context>& ctx, const sexpr::list& args) {

      if(!args) {
        compile(res, ctx, unit());
        return;
      }
        
      const context::locals_type locals = ctx->locals;
      
      bool first = true;
      for(const sexpr& arg : args) {
        if(first) first = false;
        else {
          res.push_back( opcode::POP );
        }

        compile(res, ctx, arg);
      }


      const integer n = ctx->locals.size() - locals.size();
      assert(n >= 0);
      
      if( n ) {
        // we need to clear scope

        // TODO add an opcode for that
        for(integer i = 0; i < n; ++i) {
          res.push_back(opcode::SWAP);
          res.push_back(opcode::POP);          
        }

        ctx->locals = std::move(locals);
      }
    }

    static void lambda(bytecode& res, ref<context>& ctx, const sexpr::list& args) {
      sexpr::list curr = args;
      const sexpr::list vars = head(curr).cast<sexpr::list>();

      curr = tail(curr);
      const sexpr& body = head(curr);
      
      // sub context
      ref<context> sub = make_ref<context>(ctx);

      // populate sub with locals for self + args
      if(ctx->defining) {
        sub->add_local(*ctx->defining);
      } else {
        sub->add_local("__self__");
      }

      integer n = 0;
      for(const sexpr& x : vars) {
        sub->add_local(x.get<symbol>());
        ++n;
      }

      // reserve space for return address
      sub->add_local("__return__");
      
      // label for function code
      const integer id = unique();
      
      const label start = "lambda-" + std::to_string(id);
      const label end = "after-" + start.name();

      // skip function body
      res.push_back( opcode::JMP ); 
      res.push_back( end );      

      // generate function body in sub context      
      res.label(start);
      compile(res, sub, body);
      res.push_back( opcode::RET );

      // push closure      
      res.label(end);

      // order sub captures and extend current ones
      std::vector< symbol > ordered(sub->captures.size());
      
      for(const auto& it : sub->captures) {
        const symbol& name = it.first;
        ordered[it.second] = name;
        
        auto local = ctx->locals.find(name);
        if(local == ctx->locals.end()) {
          ctx->capture(name);
        }
      }

      // load sub captures in order
      for(const symbol& s : ordered) {
        compile(res, ctx, s);
      }

      res.push_back( opcode::CLOS );
      res.push_back( integer(ordered.size()) );
      res.push_back( n );    
      res.push_back( start );      
      
    };

  
  

    static void app(bytecode& res, ref<context>& ctx, const sexpr& func, const sexpr::list& args) {

      // compile function
      compile(res, ctx, func);
    
      // compile args
      integer n = 0;
      for(const sexpr& x : args) {
        compile(res, ctx, x);
        ++n;
      }

      // call
      res.push_back( opcode::CALL );
      res.push_back( n );
    
    };
  
  
    static std::map<symbol, special> table = {
      {"def", def},
      {"lambda", lambda},
      {"cond", cond},
      {"quote", quote},
      {"do", seq},    
    };

    struct compile_visitor {

      template<class T>
      void operator()(const T& self, ref<context>& ctx, bytecode& res) const {
        throw lisp::error("codegen: not implemented");
      }
      
      // literals
      void operator()(const integer& self, ref<context>& ctx, bytecode& res) const {
        literal(res, self);
      }

      void operator()(const boolean& self, ref<context>& ctx, bytecode& res) const {
        literal(res, self);
      }

      void operator()(const unit& self, ref<context>& ctx, bytecode& res) const {
        literal(res, self);
      }
      
      
      void operator()(const ref<string>& self, ref<context>& ctx, bytecode& res) const {
        literal(res, self);
      }
    
    
      void operator()(const real& self, ref<context>& ctx, bytecode& res) const {
        literal(res, self);
      }


      void operator()(const builtin& self, ref<context>& ctx, bytecode& res) const {
        literal(res, self);
      }
    

      // vars
      void operator()(const symbol& self, ref<context>& ctx, bytecode& res) const {

        // locals
        {
          auto it = ctx->locals.find(self);
          if(it != ctx->locals.end()) {
            res.push_back( opcode::LOAD );
            res.push_back( it->second );
            return;
          }
        }

        // captures
        {
          res.push_back( opcode::LOADC );
          res.push_back( ctx->capture(self) );
          return;
        }
      
      }

      // forms
      void operator()(const sexpr::list& self, ref<context>& ctx, bytecode& res) const {
        const sexpr& h = self->head;
        
        if(h.is<symbol>()) {

          auto it = table.find(h.cast<symbol>());
          if( it != table.end() ) {
            return it->second(res, ctx, self->tail);
          }

        }

        return app(res, ctx, h, self->tail);
      }
    
    
    
    };
  }
  
  void compile(vm::bytecode& res, ref<codegen::context>& ctx, const lisp::sexpr& e) {
    e.apply(codegen::compile_visitor(), ctx, res);
  }
  

  
}
