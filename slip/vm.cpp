#include "vm.hpp"
#include <iostream>

#include "syntax.hpp"
#include "parse.hpp"

#include <sstream>

#include "tool.hpp"
#include "syntax.hpp"

#include "../indices.hpp"


namespace slip {
  namespace vm {

  
    void bytecode::label(vm::label s) {
      auto it = labels.insert( std::make_pair(s, size()) );
      if( !it.second ) throw slip::error("duplicate label: " + s.name());
    }
  
    void bytecode::link( std::size_t start )  {
      assert(start < size());
    
      for(value* x = data() + start, *end = data() + size(); x != end; ++x) {

        if(!x->is<vm::label>()) continue;
      
        const vm::label sym = x->get<vm::label>();
        auto it = labels.find( sym );
      
        if(it == labels.end()) {
          throw slip::error("unknown label: " + sym.name());
        } else {
          // std::cout << "link: " << it->first.name() << " resolved to: " << it->second << std::endl;;
          *x = it->second;
        }

      
      }
    }  

    // this is repr
    struct ostream_visitor {

      template<class T>
      void operator()(const T& self, std::ostream& out) const {
        out << self;
      }
      
      void operator()(const boolean& self, std::ostream& out) const {
        if(self) out << "true";
        else out << "false";
      }
      
      void operator()(const value::list& self, std::ostream& out) const {
        out << '(' << self << ')';
      }


      
      void operator()(const symbol& self, std::ostream& out) const {
        out << self.name();
      }

      void operator()(const label& self, std::ostream& out) const {
        out << '@' << self.name();
      }


      void operator()(const unit& self, std::ostream& out) const {
        out << "unit";
      }
      

      void operator()(const ref<string>& self, std::ostream& out) const {
        out << '"' << *self << '"';
      }
      
      
      void operator()(const builtin& self, std::ostream& out) const {
        out << "#<builtin>";
      }

      void operator()(const ref<closure>& self, std::ostream& out) const {
        out << "#<closure: @" << self->addr << ">";
      }

      void operator()(const ref<record>& self, std::ostream& out) const {
        out << "{" << make_list<value>(self->begin(), self->end()) << '}';
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

        case opcode::RECORD:
          out << "record";
          break;

        case opcode::GETATTR:
          out << "getattr";
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

        case opcode::PEEK:
          out << "peek";
          break;
          
        default:
          out << "#<opcode>";
        };
      

      }

    
    };


    void bytecode::dump(std::ostream& out, std::size_t start) const {
      assert( start < size() );
    
      std::map<integer, std::string> reverse;
      for(const auto& it : labels) {
        reverse[it.second] += it.first.name() + ":\n";
      }

      for(std::size_t i = start; i < size(); ++i) {
        const value& x = (*this)[i];
        auto it = reverse.find(i);
        if(it != reverse.end()) {
          out << std::endl << it->second;
        }
        out << '\t' << i << '\t';

        if( !x.is<opcode>() ) out << "\t";
        out << x << '\n';
      }
    }
  

    std::ostream& operator<<(std::ostream& out, const value& self) {
      self.apply( ostream_visitor(), out);
      return out;
    }  


    

    ref<closure> make_closure(std::size_t argc, std::size_t addr,
                              const value* first, const value* last) {
      return closure::create< closure >(last - first, argc, addr, first, last);
    }
    
    ref<record> make_record(const value* first, const value* last) {
      return record::create< record >(last - first, first, last);
    }



    


    // machine
    machine::machine() {
      call_stack.emplace_back( frame{0, 0, 0, 0} );
      data_stack.reserve( 1 << 10 );
    }

    

    void machine::run(const bytecode& code, std::size_t start) {

      assert( start < code.size() );

      // instruction pointer
      std::size_t ip = start;

        while(true) {
          
          switch( code[ip].get<opcode>()) {
            
          case opcode::NOOP: break;
          case opcode::STOP: return;

          case opcode::PUSH:
            // note: cannot move here
            data_stack.emplace_back( code[++ip] );
            break;
      
          case opcode::POP:
            assert( !data_stack.empty() );
            data_stack.pop_back();
            break;

          case opcode::SWAP: {
            assert( data_stack.size() > 1 );
            auto last = data_stack.rbegin();
            std::swap(*last, *(last + 1));
            break;
          }
        
          case opcode::JMP: {
            // fetch addr and jump
            ++ip;
            ip = code[ip].get<integer>();
            continue;
          }

          case opcode::JNZ: {
            // fetch addr
            const integer& addr = code[++ip].get<integer>();
            
            // pop value
            const value& top = data_stack.back();
            assert(top.is<boolean>() && "boolean expected for jnz");
            
            // jnz
            if( top.get<boolean>() ) {
              data_stack.pop_back(); // warning: top is invalidated
              ip = addr;
              continue;
            }
            
            data_stack.pop_back();
            break;
          }

        
          case opcode::LOAD: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
            assert( call_stack.back().fp + i < data_stack.size() );
            data_stack.emplace_back( data_stack[call_stack.back().fp + i]);
            break;
          }

            // store, #index
          case opcode::STORE: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
            assert( call_stack.back().fp + i < data_stack.size() );

            // pop value into cell
            value& dest = data_stack[call_stack.back().fp + i];
            dest = std::move(data_stack.back());
            data_stack.pop_back();
            break;
          }


          case opcode::LOADC: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
        
            const closure& f = *data_stack[call_stack.back().fp].get< ref<closure> >();
            assert( i < integer(f.size()) );
            data_stack.emplace_back( f[i] );
            break;
          }
        
          case opcode::STOREC: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
        
            closure& f = *data_stack[call_stack.back().fp].get< ref<closure> >();
            assert( i < integer(f.size()) );
            
            // pop value in capture
            f[i] = std::move(data_stack.back());
            data_stack.pop_back();
            break;
          }


            // clos, #capture, #arg, addr
          case opcode::CLOS: {
            // fetch and close over the last n variables
            const integer& c = code[++ip].get<integer>();
            assert(c <= integer(data_stack.size()));

            // argc
            const integer& n = code[++ip].get<integer>();
            // assert(n <= integer(data_stack.size()));
        
            // fetch code address from bytecode start
            const integer& addr = code[++ip].get<integer>();
        
            // build closure
            const std::size_t min = data_stack.size() - c;
            const value* first = &data_stack[ min ];
            const value* last = first + c;

            // TODO we should move from the data_stack            
            ref<closure> res = make_closure(n, addr, first, last);
            
            data_stack.resize(min + 1, unit());
            data_stack.back() = std::move(res);
            break;
          }


            // record, #size, magic
          case opcode::RECORD: {

            // size
            const integer& size = code[++ip].get<integer>();
            assert(size <= integer(data_stack.size()));
            
            // magic
            const integer& magic = code[++ip].get<integer>();
            // assert(n <= integer(data_stack.size()));
        
            // build record value
            const std::size_t min = data_stack.size() - size;
            
            const value* first = &data_stack[ min ];
            const value* last = first + size;

            // TODO we should move from the data_stack
            ref<record> res = make_record(first, last);
            res->magic = magic;
            
            data_stack.resize(min + 1, unit());
            data_stack.back() = std::move(res);
            break;
          }


            // getattr, hash
          case opcode::GETATTR: {

            // hash
            const std::size_t hash = code[++ip].get<integer>();
            
            assert( data_stack.back().is< ref<record> >() );
            const ref<record>& arg = data_stack.back().get< ref<record> >();

            const std::size_t index = arg->magic % hash;

            data_stack.back() = (*arg)[index];
            break;
          }            

            
            // call, #argc
          case opcode::CALL: {
            
            // calling convention:
            // (func, args..., retaddr, locals...)
            //   ^- fp
          
            // fetch argc
            const integer n = code[++ip].get<integer>();
            assert( call_stack.back().fp + n + 1 <= data_stack.size() );
        
            // get function
            const std::size_t start = data_stack.size() - (n + 1);
            const value& func = data_stack[start];
        
            switch( func.type() ) {
            case value::type_index< ref<closure> >(): {

              const std::size_t fp = start;
              const std::size_t return_addr = ip + 1;

              const ref<closure>& f = func.get<ref<closure>>();
              
              assert(f->argc == std::size_t(n));
              
              // push frame
              call_stack.emplace_back( frame{fp, return_addr, f->argc, 0} );
              
              // jump to function address
              ip = f->addr;
              continue;
              break;
            }

            case value::type_index< builtin >():

              {
                // TODO problem here if calling builtin triggers data_stack growth
                
                // call builtin
                const std::size_t first_index = start + 1;

                const value* first = data_stack.data() + first_index;
                const value* last = first + n;

                data_stack[start] = func.get<builtin>()(first, last);
                
                // pop args + push result
                data_stack.resize( first_index, unit() ); // warning: func is invalidated
              }
            
              break;
            default:
              // TODO optimize default jump test
              throw type_error("callable expected");
            };
          
            break;
          }


          case opcode::RET: {
            
            const std::size_t start = call_stack.back().fp; // - call_stack.back().argc;
            const std::size_t ret = call_stack.back().addr;
            
            data_stack[start] = std::move(data_stack.back());
            
            // cleanup frame
            data_stack.resize( start + 1, unit() );

            // TODO handle over-staturated calls
            assert(call_stack.back().cont == 0);
            
            // pop frame pointer
            call_stack.pop_back();

            // jump back
            ip = ret;
            continue;
          }

          case opcode::PEEK:
            std::clog << *this << std::endl;
            break;
            
          default:
            assert(false && "unknown opcode");
          };
        
          ++ip;
        };
    }

    std::ostream& operator<<(std::ostream& out, const machine& self) {
      out << "[";

      bool first = true;
      for(const value& x: self.data_stack) {
        if( first ) first = false;
        else out << ", ";

        out << x;
      }
      
      return out << "]";
    }
  }  
  
}



