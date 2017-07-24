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

  
    machine::machine() {
      fp.emplace_back(0);
    }

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



    



    

    void machine::run(const bytecode& code, std::size_t start) {

      assert( start < code.size() );

      std::size_t ip = start;
      
      const std::size_t init_stack_size = stack.size();
      const std::size_t init_fp_size = fp.size();    
    
      try{ 
        
        while(true) {

          switch( code[ip].get<opcode>()) {
            
          case opcode::NOOP: break;
          case opcode::STOP: return;

          case opcode::PUSH: 
            stack.emplace_back( code[++ip] );
            break;
      
          case opcode::POP:
            assert( !stack.empty() );
            stack.pop_back();
            break;

          case opcode::SWAP: {
            assert( stack.size() > 1 );
            auto last = stack.rbegin();
            std::swap(*last, *(last + 1));
            break;
          }
        
          case opcode::JMP: {
            // fetch addr and jump
            ++ip;
            ip = code[ip].get<integer>();
            continue;
            break;
          }

          case opcode::JNZ: {
            // fetch addr
            const integer& addr = code[++ip].get<integer>();
            
            // pop value
            const value& top = stack.back();

            assert(top.is<boolean>() && "boolean expected for jnz");
            
            // jnz
            if( top.get<boolean>() ) {
              stack.pop_back(); // warning: top is invalidated
              ip = addr;
              continue;
            }
            stack.pop_back();
            break;
          }

        
          case opcode::LOAD: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
            assert( fp.back() + i < stack.size() );
            stack.emplace_back( stack[fp.back() + i]);
            break;
          }

            // store, #index
          case opcode::STORE: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
            assert( fp.back() + i < stack.size() );

            // pop value into cell
            value& dest = stack[fp.back() + i];
            dest = std::move(stack.back());
            stack.pop_back();
            break;
          }


          case opcode::LOADC: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
        
            const closure& f = *stack[fp.back()].get< ref<closure> >();
            assert( i < integer(f.size()) );
            stack.emplace_back( f[i] );
            break;
          }
        
          case opcode::STOREC: {
            // fetch index
            const integer& i = code[++ip].get<integer>();
        
            closure& f = *stack[fp.back()].get< ref<closure> >();
            assert( i < integer(f.size()) );
            
            // pop value in capture
            f[i] = std::move(stack.back());
            stack.pop_back();
            break;
          }


            // clos, #capture, #arg, addr
          case opcode::CLOS: {
            // fetch and close over the last n variables
            const integer& c = code[++ip].get<integer>();
            assert(c <= integer(stack.size()));

            // argc
            const integer& n = code[++ip].get<integer>();
            // assert(n <= integer(stack.size()));
        
            // fetch code address from bytecode start
            const integer& addr = code[++ip].get<integer>();
        
            // build closure
            const std::size_t min = stack.size() - c;
            const value* first = &stack[ min ];
            const value* last = first + c;

            // TODO we should move from the stack            
            ref<closure> res = make_closure(n, addr, first, last);
            
            stack.resize(min + 1, unit());
            stack.back() = std::move(res);
            break;
          }


            // record, #size, magic
          case opcode::RECORD: {

            // size
            const integer& size = code[++ip].get<integer>();
            assert(size <= integer(stack.size()));
            
            // magic
            const integer& magic = code[++ip].get<integer>();
            // assert(n <= integer(stack.size()));
        
            // build record value
            const std::size_t min = stack.size() - size;
            
            const value* first = &stack[ min ];
            const value* last = first + size;

            // TODO we should move from the stack
            ref<record> res = make_record(first, last);
            res->magic = magic;
            
            stack.resize(min + 1, unit());
            stack.back() = std::move(res);
            break;
          }


            // getattr, hash
          case opcode::GETATTR: {

            // hash
            const std::size_t hash = code[++ip].get<integer>();
            
            assert( stack.back().is< ref<record> >() );
            const ref<record>& arg = stack.back().get< ref<record> >();

            const std::size_t index = arg->magic % hash;

            stack.back() = (*arg)[index];
            break;
          }            

            
            // call, #argc
          case opcode::CALL: {
            
            // calling convention:
            // (func, args..., retaddr, locals...)
            //   ^- fp
          
            // fetch argc
            const integer n = code[++ip].get<integer>();
            assert( fp.back() + n + 1 <= stack.size() );
        
            // get function
            const std::size_t start = stack.size() - (n + 1);
            const value& func = stack[start];
        
            switch( func.type() ) {
            case value::type_index< ref<closure> >(): {

              const closure& f = *func.get<ref<closure>>();
              
              // push frame pointer (to the closure, to be replaced with result)
              fp.emplace_back( start );
              
              // push return address
              const integer return_addr = ip + 1;
              stack.emplace_back( return_addr ); // warning: func is invalidated here

              assert(f.argc == std::size_t(n));
              
              // jump to function address
              ip = f.addr;
              continue;
              break;
            }

            case value::type_index< builtin >():

              {
                // TODO problem here if calling builtin triggers stack growth
                
                // call builtin
                const std::size_t first_index = start + 1;
                const value* first = stack.data() + first_index;
                const value* last = first + n;

                stack[start] = func.get<builtin>()(first, last);
                
                // pop args + push result
                stack.resize( first_index, unit() ); // warning: func is invalidated
              }
            
              break;
            default:
              // TODO optimize default jump test
              throw type_error("callable expected");
            };
          
            break;
          }


          case opcode::RET: {
            // put result and pop
            const std::size_t start = fp.back();
            stack[start] = std::move(stack.back());
            stack.pop_back();
        
            // pop return address
            const integer addr = stack.back().get<integer>();
            
            // cleanup frame
            stack.resize( fp.back() + 1, unit() );

            // pop frame pointer
            fp.pop_back();
            
            // jump back
            ip = addr;
            continue;
            break;
          }

          case opcode::PEEK:
            std::clog << *this << std::endl;
            break;
            
          default:
            assert(false && "unknown opcode");
          };
        
          ++ip;
        };
      } catch(slip::error& e) {
        fp.resize(init_fp_size); // should be 1 anyways?
        stack.resize(init_stack_size, unit());
        throw;
      }
      
    }

    std::ostream& operator<<(std::ostream& out, const machine& self) {
      out << "[";

      bool first = true;
      for(const value& x: self.stack) {
        if( first ) first = false;
        else out << ", ";

        out << x;
      }
      
      return out << "]";
    }
  }  
  
}



