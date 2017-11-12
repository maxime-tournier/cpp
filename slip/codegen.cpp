#include "codegen.hpp"
#include "vm.hpp"

#include "ast.hpp"

#include <sstream>

#include "../prime.hpp"

#include <iostream>

namespace slip {

  // codegen
  namespace codegen {

    using vm::bytecode;
    using vm::opcode;

    // TODO much smaller actually :-/
    static constexpr std::size_t max_record_size = 256;


    
    std::size_t variables::capture(symbol s) {
      if(!parent) throw std::runtime_error("unbound variable: " + s.str());
      
      auto res = captured.insert( std::make_pair(s, captured.size()));
      return res.first->second;
    }    

    
    // TODO 
    static std::size_t prime_hash(symbol label) {

      static const auto make_gen = [] {
        prime_enumerator<std::size_t> res;
        
        for(unsigned i = 0; i < max_record_size; ++i) {
          res();
        }

        return res;
      };
      
      static std::map<symbol, std::size_t> cache;
      static prime_enumerator<std::size_t> gen = make_gen();

      auto it = cache.find(label);
      if(it != cache.end()) return it->second;

      const std::size_t value = gen();
      cache.emplace( std::make_pair(label, value) );
      
      return value;
    }
    
  
  
    static integer unique() {
      static integer res = 0;
      return res++;
    }


   
    static void compile(vm::bytecode& res, ref<variables>& ctx, const ast::expr& e);


    static void pure(bytecode& res, ref<variables>& ctx, const sexpr::list& args) {
      // return compile(res, ctx, args->head);
    }

    
    static void set(bytecode& res, ref<variables>& ctx, const sexpr::list& args) {
      const symbol& name = args->head.get<symbol>();
      const sexpr& value = args->tail->head;
      
      if( auto index = ctx->find(name) ) {

        // compile(res, ctx, value);
        
        res.push_back( opcode::STORE );
        res.push_back( vm::instruction(*index) );

        res.push_back(opcode::PUSHU );
        // res.push_back( unit() );        

        return;
      } 

      // this should not happen with typechecking
      throw std::runtime_error("unbound variable: " + name.str());

    }

  }
    

  namespace codegen {

    struct compile_expr {

      void operator()(const ast::literal<unit>& self, vm::bytecode& res, ref<variables>& ctx) const {
        res.push_back(opcode::PUSHU);
      }

      void operator()(const ast::literal<boolean>& self, vm::bytecode& res, ref<variables>& ctx) const {
        res.push_back(opcode::PUSHB);
        res.push_back( vm::instruction(self.value) );
      }

      void operator()(const ast::literal<integer>& self, vm::bytecode& res, ref<variables>& ctx) const {
        res.push_back(opcode::PUSHI);
        res.push_back( vm::instruction(self.value) );
      }

      void operator()(const ast::literal<real>& self, vm::bytecode& res, ref<variables>& ctx) const {
        res.push_back(opcode::PUSHR);
        res.push_back( vm::instruction(self.value) );
      }
      
      void operator()(const ast::literal< ref<string> >& self, vm::bytecode& res, ref<variables>& ctx) const {
        res.push_back(opcode::PUSHS);
        res.push_back( vm::instruction(self.value->c_str() ));        
      };
      
      // template<class T>
      // void operator()(const ast::literal<T>& self, vm::bytecode& res, ref<variables>& ctx) const {
      //   res.push_back( opcode::PUSH );
      //   res.push_back( self.value );
      // }



      void operator()(const ast::variable& self, vm::bytecode& res, ref<variables>& ctx) const {
        // locals
        {
          auto it = ctx->locals.find(self.name);
          if(it != ctx->locals.end()) {
            res.push_back( opcode::LOAD );
            res.push_back( vm::instruction(it->second) );
            return;
          }
        }

        // captures
        {
          res.push_back( opcode::LOADC );
          res.push_back( vm::instruction(ctx->capture(self.name)) );
          return;
        }
      }

      
      void operator()(const ast::definition& self, vm::bytecode& res, ref<variables>& ctx) const {

        // 
        ctx->add_var(self.id);
      
        // save some space on the stack by pushing a dummy var
        res.push_back( opcode::PUSHU );
        
        // TODO better ?
        ctx->defining = &self.id;
        compile(res, ctx, self.value);
        ctx->defining = nullptr;
        
        // def evals to nil, so we just swap result and dummy var
        res.push_back( opcode::SWAP );
      }


      void operator()(const ast::binding& self, vm::bytecode& res, ref<variables>& ctx) const {
        operator()(ast::definition{self.id, self.value}, res, ctx);
      }
      
      void operator()(const ast::lambda& self, vm::bytecode& res, ref<variables>& ctx) const {
        // sub variables
        ref<variables> sub = make_ref<variables>(ctx);

        // populate sub with self arg
        if(ctx->defining) {
          sub->add_arg(*ctx->defining);
        } else {
          sub->add_arg();
        }

        // populate sub with args
        for(const ast::lambda::arg& arg : self.args) {
          const symbol s = arg.name();
            
          if(s == kw::wildcard) {
            sub->add_arg();
          } else {
            sub->add_arg(s);
          }
        }


        // label for function code
        const integer id = unique();
        
        const symbol start = "lambda-" + std::to_string(id);
        const symbol end = "after-" + start.str();

        // skip function body
        res.push_back( opcode::JMP ); 
        res.push_back( vm::instruction(end) );      

        // compile function body with sub variables      
        res.label(start);
        // res.push_back( opcode::PEEK );

        compile(res, sub, self.body);
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
          compile(res, ctx, ast::variable{s});
        }

        const std::size_t m = ordered.size(), n = size(self.args);
        
        res.push_back( opcode::CLOS );
        res.push_back( vm::instruction(m) ); 
        res.push_back( vm::instruction(n) );    
        res.push_back( vm::instruction(start) );      

      }


      void select(const ast::selection& self, vm::bytecode& res, ref<variables>& ctx) const {
        const std::size_t hash = prime_hash(self.label);

        res.push_back( opcode::GETATTR );
        res.push_back( vm::instruction(hash) );        
      }

      
      void operator()(const ast::application& self, vm::bytecode& res, ref<variables>& ctx) const {

        // compile args in reverse order
        const std::size_t n = foldr(0, self.args, [&](const ast::expr& e, std::size_t i) {
            compile(res, ctx, e);
            return i + 1;
          });

        // special applications
        if(self.func.is< ast::selection >() ) {
          select(self.func.get<ast::selection>(), res, ctx);

          // extra args
          if(n > 1) {
            res.push_back( opcode::CALL );
            res.push_back( vm::instruction(n - 1) );
          }

          return;
        }
        
        
        // compile function
        compile(res, ctx, self.func);

        // TODO optimize nullary applications
        
        // call
        res.push_back( opcode::CALL );
        res.push_back( vm::instruction(n) );
      }



      void operator()(const ast::condition& self, vm::bytecode& res, ref<variables>& ctx) const {

        const integer id = unique();

        const symbol start = "cond-" + std::to_string(id) + "-start";
      
        res.label(start);
        const symbol end = "cond-" + std::to_string(id) + "-end";
      
        std::map<symbol, ast::expr> branch;

        // compile tests
        integer i = 0;
        for(const ast::branch& b : self.branches) {
          
          const symbol then = "cond-" + std::to_string(id) +
            "-then-" + std::to_string(i);
          
          branch.emplace( std::make_pair(then, b.value) );
          
          compile(res, ctx, b.test);

          res.push_back( opcode::JNZ );
          res.push_back( vm::instruction(then) );

          ++i;
        }

        // don't forget to jump to end if nothing matched
        res.push_back( opcode::JMP );
        res.push_back( vm::instruction(end) );      

        // TODO order branches ?
        // compile branches
        for(const auto it : branch) {
          res.label(it.first);
          compile(res, ctx, it.second);
          res.push_back( opcode::JMP );
          res.push_back( vm::instruction(end) );      
        }

        res.label(end);
      }



      void operator()(const ast::sequence& self, vm::bytecode& res, ref<variables>& ctx) {
        
        if(!self.items) {
          compile(res, ctx, ast::literal<unit>());
          return;
        }
        
        const variables::locals_type locals = ctx->locals;
      
        bool first = true;

        // TODO with a fold instead?
        for(const ast::expr& arg : self.items) {
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



      void operator()(const ast::record& self, vm::bytecode& res, ref<variables>& ctx) const {

       // compile row values + build signature
        std::vector< symbol > sig;
        for(const ast::record::row& r : self.rows ) {
          compile(res, ctx, r.value);
          sig.emplace_back(r.label);
        }
        
        static std::map< std::vector<symbol>, std::size_t > magic_cache;

        const std::size_t size = sig.size();
        assert(size < max_record_size);
        
        auto it = magic_cache.find(sig);
        if( it == magic_cache.end() ) {

          // compute magic
          std::size_t a[ size ];
          std::size_t n[ size ];

          std::size_t i = 0;
          for(const ast::record::row& r : self.rows ) {
            a[i] = i;
            n[i] = prime_hash(r.label);
            ++i;
          }
          
          const std::size_t magic = chinese_remainders(a, n, size);

          it = magic_cache.emplace( std::make_pair(sig, magic)).first;
        }

        
        res.push_back( opcode::RECORD );
        res.push_back( vm::instruction(size) ) ;
        res.push_back( vm::instruction(it->second) );
      }


      
      void operator()(const ast::expr& self, vm::bytecode& res, ref<variables>& ctx) const {
        std::stringstream ss;
        ss << repr(self);
        throw error("compile: unimplemented: " + ss.str());
      }
    };
    

    static void compile(vm::bytecode& res, ref<variables>& ctx, const ast::expr& e) {
      e.apply(compile_expr(), res, ctx);
    }


    struct toplevel_visitor {
      
      void operator()(const ast::expr& self, vm::bytecode& res, ref<variables>& ctx) const {
        compile(res, ctx, self);
      }

      void operator()(const ast::module& self, vm::bytecode& res, ref<variables>& ctx) const {
        std::stringstream ss;
        ss << "not implemented: codegen for " << repr(self);
        throw error(ss.str());
      }
      
    };
    
    
    void compile(vm::bytecode& res, ref<variables>& ctx, const ast::toplevel& e) {
      e.apply(toplevel_visitor(), res, ctx);
    }

  }


  
  
}
