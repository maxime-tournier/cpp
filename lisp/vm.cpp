#include "vm.hpp"
#include <iostream>

#include "syntax.hpp"
#include "parse.hpp"

#include <sstream>

#include "tool.hpp"
#include "syntax.hpp"

namespace lisp {
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
        out << self;
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

      const std::size_t init_stack_size = data.size();
      const std::size_t init_fp_size = fp.size();    
    
      try{ 



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
            const value& top = data.back();

            // jnz
            if( top ) {
              data.pop_back();
              op = code.data() + addr;
              continue;
            }
            data.pop_back();
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
            value& dest = data[fp.back() + i];
            dest = std::move(data.back());
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


            // clos, #capture, #arg, addr
          case opcode::CLOS: {
            // fetch and close over the last n variables
            const integer c = (++op)->get<integer>();
            assert(c <= integer(data.size()));

            // argc
            const integer n = (++op)->get<integer>();
            assert(n <= integer(data.size()));
        
            // fetch code address from bytecode start
            const integer addr = (++op)->get<integer>();
        
            // build closure
            ref<closure> res = make_ref<closure>();

            res->argc = n;
            res->addr = addr;
            res->capture.reserve(c);
          
            const std::size_t size = data.size(), min = size - c;
            for(std::size_t i = min; i < size; ++i) {
              res->capture.emplace_back(std::move(data[i]));
            }
        
            data.resize(min + 1, vm::value::list());
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
            const std::size_t start = data.size() - (n + 1);
            const value& func = data[start];
        
            switch( func.type() ) {
            case value::type_index< ref<closure> >(): {

              const ref<closure>& f = func.get<ref<closure>>();
            
              // push frame pointer (to the closure, to be replaced with result)
              fp.push_back( start );
            
              // push return address
              const integer ret_addr = ++op - code.data();
              data.push_back( ret_addr);

              // TODO typecheck instead
              if( f->argc != std::size_t(n) ) {
                std::stringstream ss;
                ss << "bad argument count, expected: " << f->argc << ", got: " << n;
                throw lisp::error(ss.str());
              }

              // get function address
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

                reinterpret_cast<lisp::value&>(data[start]) = 
                  (func.get<builtin>()
                   ( reinterpret_cast<const lisp::value*>(first),
                     reinterpret_cast<const lisp::value*>(last)));
            
                // pop args + push result
                data.resize( first_index, vm::value::list() );
              }
            
              break;
            default:
              assert(false && "must call a function");
            };
          
            break;
          }

          case opcode::RET: {
            // put result and pop
            const std::size_t start = fp.back();
            data[start] = std::move(data.back());
            data.pop_back();
        
            // pop return address
            const integer addr = data.back().get<integer>();
            data.pop_back();
        
            // cleanup frame
            data.resize( fp.back() + 1, vm::value::list() );

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
      } catch(lisp::error& e) {
        fp.resize(init_fp_size); // should be 1 anyways?
        data.resize(init_stack_size, vm::value::list());
        throw e;
      }
      
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
  }  
  
}



