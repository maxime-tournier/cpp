#include "vm.hpp"
#include <iostream>

#include "syntax.hpp"
#include "parse.hpp"

#include <sstream>

#include "tool.hpp"
#include "syntax.hpp"

namespace vm {

  
  machine::machine() {
    fp.push_back(0);
  }

  void bytecode::label(vm::label s) {
    auto it = labels.insert( std::make_pair(s, size()) );
    if( !it.second ) throw lisp::error("duplicate label: " + s.name());
  }
  
  void bytecode::link( std::size_t start )  {
    assert(start < size());
    
    for(value* x = data() + start, *end = data() + size(); x != end; ++x) {

      if(!x->is<vm::label>()) continue;
      
      const vm::label sym = x->get<vm::label>();
      auto it = labels.find( sym );
      
      if(it == labels.end()) {
        throw lisp::error("unknown label: " + sym.name());
      } else {
        // std::cout << "link: " << it->first.name() << " resolved to: " << it->second << std::endl;;
        *x = it->second;
      }

      
    }
  }  

  struct value::ostream {

    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << lisp::value(self);
    }

    void operator()(const ref<closure>& self, std::ostream& out) const {
      out << "#<closure: " << self->capture.size() << ", @" << self->addr << ">";
    }

    void operator()(const opcode& self, std::ostream& out) const {

      switch(self) {
        
      case opcode::PUSH:
        out << "push";
        break;
        
      case opcode::POP:
        out << "pop";
        break;

      case opcode::SWAP:
        out << "swap";
        break;
        
      case opcode::CLOS:
        out << "clos";
        break;

      case opcode::JNZ:
        out << "jnz";
        break;
        
      case opcode::JMP:
        out << "jmp";
        break;

      case opcode::LOADC:
        out << "loadc";
        break;

      case opcode::STOREC:
        out << "storec";
        break;
        
        
      case opcode::CALL:
        out << "call";
        break;

      case opcode::RET:
        out << "ret";
        break;
        
        
      case opcode::STOP:
        out << "stop";
        break;        

      case opcode::LOAD:
        out << "load";
        break;

      case opcode::STORE:
        out << "store";
        break;
        
      default:
        out << "#<opcode>";
      };
      

    }

    
  };


  void bytecode::dump(std::ostream& out, std::size_t start) const {
    assert( start < size() );
    
    // TODO print labels
    std::map<integer, std::string> reverse;
    for(const auto& it : labels) {
      reverse[it.second] += it.first.name() + ":";
    }

    for(std::size_t i = start; i < size(); ++i) {
      const value& x = (*this)[i];
      auto it = reverse.find(i);
      if(it != reverse.end()) {
        out << std::endl << it->second << std::endl;
      }
      out << '\t' << i << '\t' << x << std::endl;
    }
  }
  

  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.apply( value::ostream(), out);
    return out;
  }  


  void machine::run(const bytecode& code, std::size_t start) {

    assert( start < code.size() );
    const value* op = code.data() + start;

    while(true) {

      switch(op->get<opcode>()) {

      case opcode::NOOP: break;
      case opcode::STOP: return;

      case opcode::PUSH: 
        data.push_back(*++op);
        break;
      
      case opcode::POP:
        assert( !data.empty() );
        data.pop_back();
        break;

      case opcode::SWAP: {
        assert( data.size() > 1 );
        auto last = data.rbegin();
        std::swap(*last, *(last + 1));
        break;
      }
        
      case opcode::JMP: {
        // fetch addr
        const integer addr = (++op)->get<integer>();
        
        // jump
        op = code.data() + addr;
        continue;
        break;
      }

      case opcode::JNZ: {
        // fetch addr
        const integer addr = (++op)->get<integer>();

        // pop value
        const value top = std::move(data.back());
        data.pop_back();
        
        // jnz
        if( top ) {
          op = code.data() + addr;
          continue;
        }
        
        break;
      }

        
      case opcode::LOAD: {
        // fetch index
        const integer i = (++op)->get<integer>();
        assert( fp.back() + i < data.size() );
        data.push_back( data[fp.back() + i]);
        break;
      }
        
      case opcode::STORE: {
        // fetch index
        const integer i = (++op)->get<integer>();
        assert( fp.back() + i < data.size() );

        // pop value into cell
        value& cell = data[fp.back() + i];
        cell = std::move(data.back());
        data.pop_back();
        break;
      }


      case opcode::LOADC: {
        // fetch index
        const integer i = (++op)->get<integer>();
        
        const ref<closure>& f = data[fp.back()].get< ref<closure> >();
        assert( i < integer(f->capture.size()) );
        data.push_back( f->capture[i] );
        break;
      }
        
      case opcode::STOREC: {
        // fetch index
        const integer i = (++op)->get<integer>();
        
        const ref<closure>& f = data[fp.back()].get< ref<closure> >();
        assert( i < integer(f->capture.size()) );

        // pop value in capture
        f->capture[i] = std::move(data.back());
        data.pop_back();
        break;
      }

        
      case opcode::CLOS: {

        // fetch argc and close over the last n variables
        const integer n = (++op)->get<integer>();
        assert(n <= integer(data.size()));
        
        // fetch code address from bytecode start
        const integer addr = (++op)->get<integer>();
        
        // build closure
        ref<closure> res = make_ref<closure>();
        
        res->addr = addr;
        res->capture.reserve(n);
          
        const std::size_t size = data.size(), min = size - n;
        for(std::size_t i = min; i < size; ++i) {
          res->capture.emplace_back(std::move(data[i]));
        }
        
        data.resize(min + 1, lisp::nil);
        data.back() = std::move(res);
        break;
      }
          
      case opcode::CALL: {

        // calling convention:
        // (func, args..., retaddr, locals...)
        //   ^- fp
          
        // fetch argc
        const integer n = (++op)->get<integer>();
        assert( fp.back() + n + 1 <= data.size() );
        
        // get function
        const std::size_t start = data.size() - n - 1;
        const value& func = data[start];
        
        switch( func.type() ) {
        case value::type_index< ref<closure> >(): {
          // push frame pointer (to closure)
          fp.push_back( start );
            
          // push return address
          const integer ret_addr = ++op - code.data();
          data.push_back( ret_addr);
          
          // get function address
          const ref<closure>& f = func.get<ref<closure>>();
          const value* addr = code.data() + f->addr;

          // jump
          op = addr;
          continue;
          break;
        }

        case value::type_index< builtin >():

          {
            // call builtin
            const std::size_t first_index = start + 1;
            const value* first = data.data() + first_index;
            const value* last = first + n;
            
            const lisp::value res = func.get<builtin>()
              ( reinterpret_cast<const lisp::value*>(first),
                reinterpret_cast<const lisp::value*>(last));
            
            // pop args + push result
            data.resize( first_index, lisp::nil );
            data.back() = std::move( reinterpret_cast<const value&>(res) );
          }
            
          break;
        default:
          assert(false && "must call a function");
        };
          
        break;
      }

      case opcode::RET: {
        // pop result
        const value result = std::move(data.back());
        data.pop_back();

        // pop return address
        const integer addr = data.back().get<integer>();
        data.pop_back();
        
        // cleanup frame
        data.resize( fp.back() + 1, lisp::nil );

        // push result
        data.back() = std::move(result);
        
        // pop frame pointer
        fp.pop_back();

        // jump back
        op = code.data() + addr;
        continue;
        break;
      }
        
      default:
        assert(false && "unknown opcode");
      };
        
      ++op;
    };

      
  }

  std::ostream& operator<<(std::ostream& out, const machine& self) {
    out << "[";

    bool first = true;
    for(const value& x: self.data) {
      if( first ) first = false;
      else out << ", ";

      out << x;
    }
    
    return out << "]";
  }
  

  static const ref<lisp::context> ctx = lisp::std_env();
  
  struct test {

    test() {
      machine m;

      bytecode code;

      auto test_print = [&] {
        code.push_back(opcode::PUSH);
        code.push_back( ctx->find("print").get<builtin>() );
      
        code.push_back( opcode::PUSH );
        code.push_back( 1l );
      
        code.push_back( opcode::PUSH );
        code.push_back( 1.5 );
      
        code.push_back( opcode::CALL );
        code.push_back( 2l );
      };      
      

      auto test_closure = [&] {

        // skip function code
        label start = "__start";
        code.push_back(opcode::JMP);        
        code.push_back(start);
        
        code.push_back(opcode::PUSH);
        code.push_back(14l);        
        code.push_back(opcode::RET);
        
        // build closure
        code.label(start);        
        code.push_back(opcode::CLOS);
        code.push_back(0l);
        code.push_back(2l);        
        
        // call closure with no arguments
        code.push_back( opcode::CALL );
        code.push_back( 0l );
        
      };      


      test_closure();

      code.push_back( opcode::STOP );

      code.link();
      m.run(code);
      
      if( !m.data.empty() ) {
        std::clog << m.data.back() << std::endl;
      }
    }  
  
  };



  // codegen
  struct context {
    ref<context> parent;

    context(const ref<context>& parent = {})
      : parent(parent) { }

    using locals_type = std::map<symbol, integer>;
    locals_type locals;
    std::map< symbol, integer > captures;    

    const symbol* defining = nullptr;
    
    integer add_local(symbol s) {
      auto res = locals.insert( std::make_pair(s, locals.size()));
      if(!res.second) throw lisp::error("duplicate local");
      return res.first->second;
    }

    integer capture(symbol s) {
      if(!parent) throw lisp::unbound_variable(s);
      
      auto res = captures.insert( std::make_pair(s, captures.size()));
      return res.first->second;
    }
    
  };


  using special = void (*)(bytecode& res, ref<context>&, const list&);

  
  static void compile(bytecode& res, ref<context>& ctx, const lisp::value& e);

  
  static void def(bytecode& res, ref<context>& ctx, const list& args) {
    try{
      list curr = args;
      const symbol name = head(curr).cast<symbol>();
      ctx->add_local(name);
      
      curr = tail(curr);
      const lisp::value& expr = head(curr);

      // TODO better ?
      ctx->defining = &name;
      compile(res, ctx, expr);
      ctx->defining = nullptr;

      // def evals to nil
      res.push_back( opcode::PUSH );
      res.push_back( lisp::nil );
      
    } catch( lisp::error& e ) {
      throw std::logic_error("def");
    }
  };

  static integer unique() {
    static integer res = 0;
    return res++;
  }


  static void cond(bytecode& res, ref<context>& ctx, const list& args) {
    try{

      const integer id = unique();


      const label start = "cond-" + std::to_string(id) + "-start";
      
      res.label(start);
      const label end = "cond-" + std::to_string(id) + "-end";
      
      std::map<label, lisp::value> branch;

      // compile tests
      integer i = 0;
      for(const lisp::value& item : args) {
        
        const list chunk = item.cast<list>(); 

        const lisp::value test = head(chunk);
        const lisp::value result = head(tail(chunk));        
        // TODO syntax check

        const label then = "cond-" + std::to_string(id) + "-then-" + std::to_string(i);
        
        branch.emplace( std::make_pair(then, result) );
        
        compile(res, ctx, test);
        res.push_back( opcode::JNZ );
        res.push_back( then );
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
      
    } catch( lisp::error& e ) {
      throw lisp::syntax_error("lambda");
    }

  }

  template<class T>
  static void literal(bytecode& res, const T& value) {
    res.push_back( opcode::PUSH );
    res.push_back( value );
  }

  
  

  static void quote(bytecode& res, ref<context>& ctx, const list& args) {
    try{
      const lisp::value arg = head(args);
      // TODO syntax check

      literal(res, reinterpret_cast<const value&>(arg));
      
    } catch(lisp::error& e) {
      throw lisp::syntax_error("quote");
    }
  }

  static void seq(bytecode& res, ref<context>& ctx, const list& args) {
    try{
      const context::locals_type locals = ctx->locals;
      
      bool first = true;
      for(const lisp::value& arg : args) {
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

      
    } catch( lisp::error& e ) {
      throw lisp::syntax_error("seq");
    }
    
  }

  static void lambda(bytecode& res, ref<context>& ctx, const list& args) {
    try{
      list curr = args;
      const list args = head(curr).cast<list>();

      curr = tail(curr);
      const lisp::value& body = head(curr);

      // sub context
      ref<context> sub = make_ref<context>(ctx);

      // populate sub with locals for self + args
      if(ctx->defining) {
        sub->add_local(*ctx->defining);
      } else {
        sub->add_local("__self__");
      }

      for(const lisp::value& x : args) {
        sub->add_local(x.get<symbol>());
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
      res.push_back( start );      
      
    } catch( lisp::error& e ) {
      throw lisp::syntax_error("lambda");
    }
  };

  
  

  static void app(bytecode& res, ref<context>& ctx, const lisp::value& func, const list& args) {

    // compile function
    compile(res, ctx, func);
    
    // compile args
    integer n = 0;
    for(const lisp::value& x : args) {
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
      throw lisp::error("not implemented");
    }

    // literals
    void operator()(const integer& self, ref<context>& ctx, bytecode& res) const {
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
    void operator()(const list& self, ref<context>& ctx, bytecode& res) const {
      const lisp::value& h = head(self);

      if(h.is<symbol>()) {

        auto it = table.find(h.cast<symbol>());
        if( it != table.end() ) {
          return it->second(res, ctx, tail(self));
        }

      }

      return app(res, ctx, h, tail(self));
    }
    
    
    
  };
  
  static void compile(bytecode& res, ref<context>& ctx, const lisp::value& e) {
    e.apply(compile_visitor(), ctx, res);
  }
  

  struct test_codegen {
    test_codegen() {

      machine m;

      bytecode code;
      ref<context> ctx = make_ref<context>();
      
      const auto parser = *lisp::parser() >> [&]( std::deque<lisp::value>&& exprs) {

        for(const lisp::value& e : exprs) {
          std::cout << "compiling: " << e << std::endl;
          compile(code, ctx, e);
        }
        
        return parse::pure(exprs);
      };

      std::stringstream ss;

      ss << "(def y 5)";
      ss << "(def second (lambda (x y) y))";
      ss << "(def test (lambda (x) (lambda (y) x)))";
      ss << "(def foo (test 2))";
      ss << "(def nil '())";
      ss << "(cond (nil 0) (nil 1) ('else 2))";
      ss << "(seq 1 2 3)";
      
      try {
        if( !parser(ss) || !ss.eof() ) {
          throw lisp::error("parse error");
        }

        code.push_back(opcode::STOP);
      
        std::cout << "bytecode:" << std::endl;
        std::cout << code << std::endl;

        code.link();
        m.run(code);

        if(m.data.size()) {
          std::cout << m.data.back() << std::endl;
        }

      } catch( lisp::error& e ) {
        std::cerr << "error: " << e.what() << std::endl;
      }
    }


  };


  jit::jit()
    : ctx( make_ref<context>()),
      env( make_ref<lisp::context>()) { }
  
  jit::~jit() { }

  void jit::import(const ref<lisp::context>& env) {

    this->env->locals.insert(env->locals.begin(),
                             env->locals.end());
    
    for(const auto& it : env->locals) {
      eval( symbol("def") >>= it.first >>= it.second >>= lisp::nil );
    }

  }
  
  lisp::value jit::eval(const lisp::value& expr) {
    const std::size_t start = code.size();

    // macro 
    const lisp::value ex = expand(env, expr);
    
    compile(code, ctx, expr);
    code.push_back( opcode::STOP );

    std::cout << "bytecode:" << std::endl;
    code.dump(std::cout, start);
    
    code.link(start);

    m.run(code, start);
    const value res = std::move(m.data.back());
    m.data.pop_back();
    
    return reinterpret_cast<const lisp::value&>(res);    
  }
  
}



