#include "vm.hpp"
#include <iostream>

#include "parse.hpp"
#include <sstream>
  
namespace vm {

  static const auto print = +[](const lisp::value* first, const lisp::value* last) -> lisp::value {
    bool start = true;
    for(const lisp::value* it = first; it != last; ++it) {
      if(start) start = false;
      else std::cout << ' ';
      std::cout << *it;
    }
    std::cout << std::endl;
    return lisp::nil;
  };


  machine::machine() {
    fp.push_back(0);
  }

  void bytecode::label(vm::label s) {
    auto it = labels.insert( std::make_pair(s, size()) );
    if( !it.second ) throw lisp::error("duplicate label: " + s.name());
  }
  
  void bytecode::link()  {
    for(value& x : *this) {
    
      if(!x.is<vm::label>()) continue;

      const vm::label sym = x.get<vm::label>();
      auto it = labels.find( sym );
      
      if(it == labels.end()) {
        throw lisp::error("unknown label: " + sym.name());
      } else {
        std::cout << "label: " << it->first.name() << " resolved to: " << it->second << std::endl;;
        x = it->second;
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

  std::ostream& operator<<(std::ostream& out, const bytecode& self) {
    // TODO print labels
    std::map<integer, symbol> reverse;
    for(const auto& it : self.labels) {
      reverse[it.second] = it.first;
    }
    
    std::size_t i = 0;
    for(const value& x : self) {
      auto it = reverse.find(i);
      if(it != reverse.end()) {
        out << std::endl << it->second.name() << ":" << std::endl;
      }
      out << '\t' << i++ << '\t' << x << std::endl;
    }

    return out;
  }
  

  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.apply( value::ostream(), out);
    return out;
  }  


  void machine::run(const bytecode& code) {

    const value* op = code.data();

    while(true) {

      std::clog << op - code.data() << " " << *op << std::endl;
      
      switch(op->get<opcode>()) {

      case opcode::NOOP: break;
      case opcode::STOP: return;
      case opcode::PUSH: data.push_back(*++op); break;
      case opcode::POP: data.pop_back(); break;

      case opcode::JMP: {
        // fetch addr
        ++op;
        const integer addr = op->get<integer>();

        // jump
        op = code.data() + addr;
        continue;
        break;
      }

      case opcode::JNZ: {
        // fetch addr
        ++op;
        const integer addr = op->get<integer>();

        // pop value
        const value top = std::move(data.back());

        // jnz
        if( top ) {
          op = code.data() + addr;
          continue;
        }
        
        break;
      }

        
      case opcode::LOAD: {
        // fetch index
        ++op;
        const integer i = op->get<integer>();
        assert( fp.back() + i < data.size() );
        data.push_back( data[fp.back() + i]);
        break;
      }
        
      case opcode::STORE: {
        // fetch index
        ++op;
        const integer i = op->get<integer>();
        assert( fp.back() + i < data.size() );

        // pop value into cell
        value& cell = data[fp.back() + i];
        cell = std::move(data.back());
        data.pop_back();
        break;
      }


      case opcode::LOADC: {
        // fetch index
        ++op;
        const integer i = op->get<integer>();

        const ref<closure>& f = data[fp.back()].get< ref<closure> >();
        assert( i < integer(f->capture.size()) );
        data.push_back( f->capture[i] );
        break;
      }
        
      case opcode::STOREC: {
        // fetch index
        ++op;
        const integer i = op->get<integer>();

        const ref<closure>& f = data[fp.back()].get< ref<closure> >();
        assert( i < integer(f->capture.size()) );

        // pop value in capture
        f->capture[i] = std::move(data.back());
        data.pop_back();
        break;
      }

        
      case opcode::CLOS: {

        // fetch argc and close over the last n variables
        ++op;
        const integer n = op->get<integer>();
        assert(n <= integer(data.size()));
        
        // fetch code address from bytecode start
        ++op;
        const integer addr = op->get<integer>();

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
        ++op;
        const integer n = op->get<integer>();
        assert( fp.back() + n + 1 <= data.size() );
        
        // get function
        const std::size_t start = data.size() - n - 1;
        const value func = data[start];
        
        switch( func.type() ) {
        case value::type_index< ref<closure> >(): {
          // push frame pointer (to closure)
          fp.push_back( start );
            
          // push return address
          data.push_back(++op - code.data());

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
            const value* first = data.data() + start + 1;
            const value* last = first + n;
            
            const lisp::value res = func.get<builtin>()
              ( reinterpret_cast<const lisp::value*>(first),
                reinterpret_cast<const lisp::value*>(last));
            
            // pop args + push result
            data.resize( start, lisp::nil );
            data.push_back( reinterpret_cast<const value&>(res) );
          }
            
          break;
        default:
          assert(false && "must call a function");
        };
          
        break;
      }

      case opcode::RET: {
        // pop result
        value result = std::move(data.back());
        data.pop_back();

        // pop return address
        const integer addr = data.back().get<integer>();
        data.pop_back();
        
        // cleanup frame
        data.resize( fp.back(), lisp::nil );

        // push result
        data.emplace_back( std::move(result) );

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

  
  static struct test {

    test() {
      machine m;

      bytecode code;

      auto test_print = [&] {
        code.push_back(opcode::PUSH);
        code.push_back( print );
      
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
  
  } instance;



  // codegen
  struct context {
    ref<context> parent;

    context(const ref<context>& parent = {}) : parent(parent) { }
    
    std::map< symbol, integer > locals;
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
      
    } catch( lisp::error& e ) {
      throw lisp::syntax_error("def");
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
    
    // TODO seq
    // TODO seq create/destroy a scope (but not a frame?)
    // TODO how does seq mix with def ?
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
  

  static struct test_codegen {
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
      // ss << "(foo 10)";
      ss << "(def nil (quote ()))";
      ss << "(cond (nil 0) (nil 1) (2 2))";
      
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


  } instance2;
  
}



