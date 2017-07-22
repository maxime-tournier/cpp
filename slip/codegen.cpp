#include "codegen.hpp"
#include "vm.hpp"

#include "syntax.hpp"
#include "ast.hpp"

#include <sstream>

namespace slip {

  // codegen
  namespace codegen {

    using vm::bytecode;
    using vm::label;
    using vm::opcode;


  
  
    static integer unique() {
      static integer res = 0;
      return res++;
    }


   
    static void compile(vm::bytecode& res, ref<variables>& ctx, const ast::expr& e);


    static void quote(bytecode& res, ref<variables>& ctx, const sexpr::list& args) {
      // literal(res, args->head);
    }


    static void pure(bytecode& res, ref<variables>& ctx, const sexpr::list& args) {
      // return compile(res, ctx, args->head);
    }

    
    static void set(bytecode& res, ref<variables>& ctx, const sexpr::list& args) {
      const symbol& name = args->head.get<symbol>();
      const sexpr& value = args->tail->head;
      
      if( std::size_t* index = ctx->find(name) ) {

        // compile(res, ctx, value);
        
        res.push_back( opcode::STORE );
        res.push_back( integer(*index) );

        res.push_back(opcode::PUSH );
        res.push_back( unit() );        

        return;
      } 

      // this should not happen with typechecking
      throw unbound_variable(name);

    }

  }
    

  namespace codegen {

    struct compile_expr {

      template<class T>
      void operator()(const ast::literal<T>& self, vm::bytecode& res, 
                      ref<variables>& ctx) const {
        res.push_back( opcode::PUSH );
        res.push_back( self.value );
      }



      void operator()(const symbol& self, vm::bytecode& res, ref<variables>& ctx) const {
        // locals
        {
          auto it = ctx->locals.find(self);
          if(it != ctx->locals.end()) {
            res.push_back( opcode::LOAD );
            res.push_back( integer(it->second) );
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

      
      void operator()(const ref<ast::definition>& self, vm::bytecode& res, ref<variables>& ctx) const {
        
        ctx->add_local(self->id);
      
        // save some space on the stack by pushing a dummy var
        res.push_back( opcode::PUSH );
        res.push_back( unit() );
      
        // TODO better ?
        ctx->defining = &self->id;
        compile(res, ctx, self->value);
        ctx->defining = nullptr;
        
        // def evals to nil, so we just swap result and dummy var
        res.push_back( opcode::SWAP );
      }

      
      void operator()(const ref<ast::lambda>& self, vm::bytecode& res, ref<variables>& ctx) const {
        // sub variables
        ref<variables> sub = make_ref<variables>(ctx);

        // populate sub with locals for self
        if(ctx->defining) {
          sub->add_local(*ctx->defining);
        } else {
          sub->add_local("__self__");
        }

        // populate sub with args
        for(const symbol& s : self->args) {
          sub->add_local(s);
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

        // generate function body with sub variables      
        res.label(start);

        compile(res, sub, self->body);
        res.push_back( opcode::RET );
        
        // push closure      
        res.label(end);

        // order sub captures and extend current ones
        std::vector< symbol > ordered(sub->captured.size());
      
        for(const auto& it : sub->captured) {
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

        const integer m = ordered.size(), n = size(self->args);
        
        res.push_back( opcode::CLOS );
        res.push_back( m ); 
        res.push_back( n );    
        res.push_back( start );      

      }


      void operator()(const ref<ast::application>& self, vm::bytecode& res, ref<variables>& ctx) const {

        // compile function
        compile(res, ctx, self->func);
        
        // compile args
        integer n = 0;
        for(const ast::expr& e : self->args) {
          compile(res, ctx, e);
          ++n;
        }

        // call
        res.push_back( opcode::CALL );
        res.push_back( n );
      }



      void operator()(const ast::condition& self, vm::bytecode& res, ref<variables>& ctx) const {

        const integer id = unique();

        const label start = "cond-" + std::to_string(id) + "-start";
      
        res.label(start);
        const label end = "cond-" + std::to_string(id) + "-end";
      
        std::map<label, ast::expr> branch;

        // compile tests
        integer i = 0;
        for(const ast::branch& b : self.branches()) {
          
          const label then = "cond-" + std::to_string(id) +
            "-then-" + std::to_string(i);
          
          branch.emplace( std::make_pair(then, b.value) );
          
          compile(res, ctx, b.test);
          res.push_back( opcode::JNZ );
          res.push_back( then );

          ++i;
        }

        // don't forget to jump to end if nothing matched
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



      void operator()(const ast::sequence& self, vm::bytecode& res, ref<variables>& ctx) {
        
        if(!self) {
          compile(res, ctx, ast::literal<unit>());
          return;
        }
        
        const variables::locals_type locals = ctx->locals;
      
        bool first = true;

        for(const ast::expr& arg : self.items()) {
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
      
      
      template<class T>
      void operator()(const T& self, vm::bytecode& res, ref<variables>& ctx) const {
        std::stringstream ss;
        ss << self;
        throw error("compile: unimplemented: " + ss.str());
      }
    };
    

    static void compile(vm::bytecode& res, ref<variables>& ctx, const ast::expr& e) {
      e.apply(compile_expr(), res, ctx);
    }
    
    
    void compile(vm::bytecode& res, ref<variables>& ctx, const ast::toplevel& e) {
      compile(res, ctx, e.get<ast::expr>());
    }

  }


  
  
}
