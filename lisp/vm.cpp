#include "vm.hpp"
#include <iostream>

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


  void bytecode::label(vm::label s) {
    auto it = labels.insert( std::make_pair(s, size()) );
    if( !it.second ) throw lisp::error("duplicate label: " + s.name());
  }
  
  void bytecode::link()  {
    for(value& x : *this) {
    
      if(!x.is<vm::label>()) continue;
    
      auto it = labels.find(x.get<vm::label>());
      if(it == labels.end()) {
        throw lisp::error("unknown label: " + it->first.name());
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
      out << "#<opcode>";
    }

    
  };  

  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.apply( value::ostream(), out);
    return out;
  }  


  void machine::run(const bytecode& code) {

    const value* op = code.data();

    while(true) {

      std::clog << op - code.data() << " " << *op << std::endl;
      assert(op->is<opcode>());
        
      switch(op->get<opcode>()) {

      case opcode::NOOP: break;
      case opcode::STOP: return;
      case opcode::PUSH: data.push_back(*++op); break;
      case opcode::POP: data.pop_back(); break;
      case opcode::JMP: {
        // fetch addr
        ++op; assert( op->is<lisp::integer>());

        // jump
        op = code.data() + op->get<lisp::integer>();
        continue;
        break;
      }
      case opcode::LOAD:
        // fetch index
        ++op; assert( op->is<lisp::integer>());
        data.push_back( data[fp.back() + op->get<lisp::integer>()]);
        break;
      case opcode::STORE: {
        // fetch index
        ++op; assert( op->is<lisp::integer>());
        auto& dst = data[fp.back() + op->get<lisp::integer>()];
        dst = *++op;
        break;
      }
        // case opcode::BUILTIN: {

        //   // fetch argc 
        //   ++op; assert( op->is<lisp::integer>());
        //   const lisp::integer n = op->get<lisp::integer>();
        //   assert(n <= data.size());
          
        //   // builtin
        //   assert( data.end()[- n - 1].is<lisp::builtin>());
        //   const lisp::builtin func = data.end()[- n - 1].get<lisp::builtin>();
          
        //   // call builtin
        //   const lisp::value res = func( reinterpret_cast<const lisp::value*>(&data.end()[-n]),
        //                                 reinterpret_cast<const lisp::value*>(&data.end()[0]) );

        //   // pop args + push result
        //   data.resize( data.size() - n - 1, lisp::nil );
        //   data.push_back( reinterpret_cast<const value&>(res) );
        //   break;
        // }
      case opcode::CLOSE: {

        // fetch argc and close over the last n variables
        ++op; assert( op->is<lisp::integer>());
        const lisp::integer n = op->get<lisp::integer>();
        assert(n <= data.size());

        // fetch code address from bytecode start
        ++op; assert( op->is<lisp::integer>());
        const lisp::integer addr = op->get<lisp::integer>();

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
        // ^- fp
          
        // fetch argc
        ++op; assert( op->is<lisp::integer>());
        const lisp::integer n = op->get<lisp::integer>();


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
        case value::type_index< lisp::builtin >():

          {
            // call builtin
            const lisp::value res = func.get<lisp::builtin>()
              ( reinterpret_cast<const lisp::value*>(&data.end()[-n]),
                reinterpret_cast<const lisp::value*>(&data.end()[0]) );
              
            // pop args + push result
            data.resize( data.size() - n - 1, lisp::nil );
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
        value addr = std::move(data.back());
        data.pop_back();
        assert(addr.is<lisp::integer>());

        // cleanup frame
        data.resize( fp.back(), lisp::nil );

        // push result
        data.emplace_back( std::move(result) );

        // pop frame pointer
        fp.pop_back();

        // jump back
        op = code.data() + addr.get<lisp::integer>();
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
        code.push_back(opcode::CLOSE);
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
      
      std::clog << "success" << std::endl;
      if( !m.data.empty() ) {
        std::clog << m.data.back() << std::endl;
      }
    }  
  
  } instance;
  
}



