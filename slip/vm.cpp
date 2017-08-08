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


    struct stack : machine::data_stack_type { };

    value pop(stack* self) {
      assert(self->size() > 0);
      value res = std::move(self->back());
      self->pop_back();
      return res;
    }


    template<class Iterator>
    std::ostream& peek(std::ostream& out, Iterator first, Iterator last) {
      out << '[';
      bool flag = true;
      for(Iterator it = first; it != last; ++it) {
        if( flag ) flag = false;
        else out << ", ";

        out << *it;
      }
      return out << ']';
    }
    


    static const std::size_t opcode_argc(opcode op) {

      static constexpr std::size_t n = std::size_t(opcode::OPCODE_COUNT);

      
      static const std::pair<opcode, std::size_t> info [n] = {
        {opcode::NOOP, 0},
        {opcode::HALT, 0},

        // {opcode::PUSH, 1},
        {opcode::PUSHU, 0},

        {opcode::PUSHB, 1},
        {opcode::PUSHI, 1},
        {opcode::PUSHR, 1},                    
        
        {opcode::POP, 0},

        {opcode::SWAP, 0},

        {opcode::LOAD, 1},
        {opcode::STORE, 1},      
      
        {opcode::CALL, 1},
        {opcode::RET, 0},      

        {opcode::JNZ, 1},
        {opcode::JMP, 1},
      
        {opcode::LOADC, 1},
        {opcode::STOREC, 1},      
        {opcode::CLOS, 3},

        {opcode::RECORD, 3},
        {opcode::GETATTR, 1},      
      
      };
      
      static std::size_t table[n];

      static const bool init = [] {
        for(std::size_t i = 0; i < n; ++i) {
          table[ std::size_t(info[i].first) ] = info[i].second;
        }
        return true;
      }(); (void) init;
      
      return table[std::size_t(op)];
    };
    
    
    
    void bytecode::label(symbol s) {
      auto it = labels.insert( std::make_pair(s, size()) );
      if( !it.second ) throw slip::error("duplicate label: " + s.name());
    }


    void bytecode::link( std::size_t start )  {
      assert(start < size());

      const auto resolve = [&](instruction& instr) {
        auto it = labels.find( instr.label );

        if(it == labels.end()) {
          throw slip::error("unknown label: " + instr.label.name());
        } else {
          // std::cout << "link: " << it->first.name() << " resolved to: " << it->second << std::endl;;
          instr.value = it->second;
        }
        
      };

      
      for(instruction* x = data() + start, *end = data() + size(); x != end; x += 1 + opcode_argc(x->op) ) {

        switch( x->op ) {
        case opcode::JMP:
        case opcode::JNZ:
          resolve(x[1]);
          break;
        case opcode::CLOS:
          resolve(x[3]);
          break;
        default:
          break;
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

      void operator()(const ref<partial>& self, std::ostream& out) const {
        out << "#<partial>";
      }

      
      void operator()(const ref<record>& self, std::ostream& out) const {
        out << "{" << make_list<value>(self->begin(), self->end()) << '}';
      }
      

    
    };



    const instruction* dump_instr(std::ostream& out, const instruction* self) {
      
      switch(self->op) {
        // case opcode::PUSH:
        //   out << "push";
        //   break;

      case opcode::PUSHU:
        out << "pushu";
        break;
          
      case opcode::PUSHB:
        out << "pushb" << "\t" << ((++self)->as_boolean? "true" : "false");
        break;
          
      case opcode::PUSHI:
        out << "pushi" << "\t" << (++self)->as_integer;
        break;
          
      case opcode::PUSHR:
        out << "pushr" << "\t" << (++self)->as_real;
        break;
          
      case opcode::POP:
        out << "pop"; 
        break;

      case opcode::SWAP:
        out << "swap"; 
        break;
        
      case opcode::CLOS:
        out << "clos";
        out << "\t" << (++self)->value;
        out << " " << (++self)->value;
        out << " " << "@" << (++self)->label;
        break;

      case opcode::RECORD:
        out << "record";
        out << "\t" << (++self)->value;
        out << " " << (++self)->value;
        break;

      case opcode::GETATTR:
        out << "getattr" << "\t" << (++self)->value;
        break;
          
      case opcode::JNZ:
        out << "jnz" << "\t" << "@" << (++self)->label;
        break;
        
      case opcode::JMP:
        out << "jmp" << "\t" << "@" << (++self)->label;
        break;

      case opcode::LOADC:
        out << "loadc" << "\t" << (++self)->value;
        break;

      case opcode::STOREC:
        out << "storec" << "\t" << (++self)->value;
        break;
        
        
      case opcode::CALL:
        out << "call" << "\t" << (++self)->value;
        break;

      case opcode::RET:
        out << "ret";
        break;
        
        
      case opcode::HALT:
        out << "halt";
        break;        

      case opcode::LOAD:
        out << "load" << "\t" << (++self)->as_integer;
        break;

      case opcode::STORE:
        out << "store" << "\t" << (++self)->as_integer;
        break;

      case opcode::PEEK:
        out << "peek";
        break;
          
      default:
        out << "#<opcode>";
      };
      
      return self;
    }

    

    void bytecode::dump(std::ostream& out, std::size_t start) const {
      assert( start < size() );
    
      std::map<integer, std::string> reverse;
      for(const auto& it : labels) {
        reverse[it.second] += it.first.name() + ":\n";
      }

      for(const instruction* i = data() + start; i < data() + size(); ++i) {
        const std::size_t off = i - (data() + start);

        // print label
        auto it = reverse.find(off);
        if(it != reverse.end()) {
          out << std::endl << it->second;
        }
        out << '\t' << off << '\t';

        i = dump_instr(out, i);
        out << std::endl;
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
    
    ref<record> make_record(std::size_t magic, const value* first, const value* last) {
      return record::create< record >(last - first, magic, first, last);
    }


    ref<partial> make_partial(const value* first, const value* last) {
      return partial::create< partial >(last - first, first, last);
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

      // argc register
      std::size_t call_argc = 0;
      
        while(true) {
          
          switch( code[ip].op ) {
            // noop
          case opcode::NOOP: break;

            // halt
          case opcode::HALT: return;

            // push #value
          // case opcode::PUSH:
          //   data_stack.emplace_back( code[++ip].value );
          //   break;

          case opcode::PUSHU:
            data_stack.emplace_back( unit() );
            break;
          case opcode::PUSHB:
            data_stack.emplace_back( code[++ip].as_boolean );
            break;
          case opcode::PUSHI:
            data_stack.emplace_back( code[++ip].as_integer );
            break;
          case opcode::PUSHR:
            data_stack.emplace_back( code[++ip].as_real );
            break;

            
            
            // pop
          case opcode::POP:
            assert( !data_stack.empty() );
            data_stack.pop_back();
            break;

            // swap
          case opcode::SWAP: {
            assert( data_stack.size() > 1 );
            auto last = data_stack.rbegin();
            std::swap(*last, *(last + 1));
            break;
          }

            // jmp @addr
          case opcode::JMP: {
            const std::size_t addr = code[++ip].value;
            ip = addr;
            continue;
          }

            // jnz @addr
          case opcode::JNZ: {
            // std::clog << ip << " " << *this << std::endl;
            
            const std::size_t addr = code[++ip].value;
            
            // pop value
            const value top = std::move(data_stack.back());
            data_stack.pop_back();
            
            assert(top.is<boolean>() && "boolean expected for jnz");

            // jnz
            if( top.get<boolean>() ) {
              ip = addr;
              continue;
            }
            
            break;
          }


            // load #index
          case opcode::LOAD: {
            const integer index = code[++ip].as_integer;
            assert( call_stack.back().fp + index < data_stack.size() );
            assert( index >= - integer(call_stack.back().argc) - 1);
            
            data_stack.emplace_back( data_stack[call_stack.back().fp + index]);
            break;
          }

            // store #index
          case opcode::STORE: {
            const integer index = code[++ip].as_integer;
            assert( call_stack.back().fp + index < data_stack.size() );
            assert( index >= -integer(call_stack.back().argc) - 1);

            // pop value into cell
            data_stack[call_stack.back().fp + index] = std::move(data_stack.back());
            data_stack.pop_back();
            break;
          }


            // loadc #index
          case opcode::LOADC: {
            const std::size_t index = code[++ip].value;

            // std::clog << ip << " " << *this << std::endl;
            
            const ref<closure>& f = data_stack[call_stack.back().fp - 1].get< ref<closure> >();
            assert( index < f->size() );
            
            data_stack.emplace_back( (*f)[index] );
            break;
          }

            // storec #index            
          case opcode::STOREC: {
            const std::size_t index = code[++ip].value;
        
            ref<closure>& f = data_stack[call_stack.back().fp - 1].get< ref<closure> >();
            assert( index < f->size() );
            
            // pop value in capture
            (*f)[index] = std::move(data_stack.back());
            data_stack.pop_back();
            break;
          }


            // clos #size #argc @addr
          case opcode::CLOS: {
            const std::size_t size = code[++ip].value;
            assert( call_stack.back().fp + size <= data_stack.size() );            
            
            // argc
            const std::size_t argc = code[++ip].value;
            
            // fetch code address from bytecode start
            const std::size_t addr = code[++ip].value;
            
            // build closure
            const std::size_t start = data_stack.size() - size;
            
            const value* first = data_stack.data() + start;
            const value* last = first + size;
            
            // TODO we should move from data_stack
            ref<closure> res = make_closure(argc, addr, first, last);

            data_stack.resize(start + 1, unit());            
            data_stack[start] = std::move(res);            
            break;
          }


            // record #size #magic
          case opcode::RECORD: {

            // size
            const std::size_t size = code[++ip].value;
            assert( call_stack.back().fp + size < data_stack.size() );                        
            
            // magic
            const std::size_t magic = code[++ip].value;
            
            // build record value
            const std::size_t start = data_stack.size() - size;
            
            const value* first = &data_stack[ start ];
            const value* last = first + size;
            
            // TODO we should move from the data_stack
            ref<record> res = make_record(magic, first, last);
            data_stack.resize(start + 1, unit());
            
            data_stack[start] = std::move(res);
            break;
          }


            // getattr #hash
          case opcode::GETATTR: {

            // hash
            const std::size_t hash = code[++ip].value;
            
            assert( data_stack.back().is< ref<record> >() );
            const ref<record>& arg = data_stack.back().get< ref<record> >();

            const std::size_t index = arg->magic % hash;

            data_stack.back() = (*arg)[index];
            break;
          }            

            
            // call #argc
          case opcode::CALL: 
            
            // calling convention:
            // (func, args..., retaddr, locals...)
            //   ^- fp
            
            // (a_n, ... a_1, f, locals...)
            //                   ^- fp
          
            // fetch argc
            call_argc = code[++ip].value;

          call: {
            assert( call_stack.back().fp + call_argc + 1 <= data_stack.size() );

            // get function
            const value& func = data_stack.back();
            
            switch( func.type() ) {
            case value::type_index< ref<closure> >(): {

              const ref<closure>& f = func.get<ref<closure>>();
              const int cont = int(call_argc) - int(f->argc);

              if( cont < 0 ) {
                // TODO this also works for builtins
                // partial application: save stack slice
                const std::size_t start = data_stack.size() - (call_argc + 1);
                
                // put result and shrink stack
                ref<partial> res = make_partial(data_stack.data() + start,
                                                data_stack.data() + data_stack.size());
                data_stack.resize(start + 1, unit());
                data_stack[start] = std::move(res);                  
                break;
              }

              const std::size_t fp = data_stack.size();
              const std::size_t return_addr = ip + 1;

              // push frame
              call_stack.emplace_back( frame{fp, return_addr, f->argc, std::size_t(cont)} );
              
              // jump to function address
              ip = f->addr;
              continue;
            }

            case value::type_index< builtin >(): {
              const builtin ptr = func.get<builtin>();
                
              // pop self off the stack
              data_stack.pop_back();
                
              // call builtin
              const std::size_t start = data_stack.size() - call_argc;
                
              stack* args = static_cast<stack*>(&data_stack);
              value result = ptr(args);
              assert( data_stack.size() >= start && "function popped too many args");
                
              // pop args + push result
              data_stack.resize( start + 1, unit() );
              data_stack[start] = std::move(result);
              break;
            }

            case value::type_index< ref<partial> >(): {
              const ref<partial> self = func.get< ref<partial> >();

              // pop self off the stack
              data_stack.pop_back();

              // unpack stack slice
              for(const value& x : *self) {
                // note: cannot move x since it may be used multiple times
                data_stack.emplace_back(x);
              }
              
              // increment call_argc
              call_argc += (self->size() - 1);

              // and retry with this call
              goto call;
            }
              
            default:
              // TODO optimize default jump test
              throw error("callable expected");
            };
          
            break;
          }

            // ret
          case opcode::RET: {

            // move result where it belongs
            const std::size_t start = call_stack.back().fp - (call_stack.back().argc + 1);
            data_stack[start] = std::move(data_stack.back());
            
            // shrink stack
            data_stack.resize( start + 1, unit() );
            
            // return address
            const std::size_t ret = call_stack.back().addr;
            const std::size_t cont = call_stack.back().cont;

            // pop frame pointer            
            call_stack.pop_back();
            
            if(cont > 0) {
              ip = ret - 1;
              call_argc = cont;
              // std::clog << "continuing with: " << cont << std::endl;
              goto call;
            }
            
            // jump back
            ip = ret;
            continue;
          }

            // peek
          case opcode::PEEK:
            // peek current stack frame
            peek(std::cout,
                 data_stack.data() + call_stack.back().fp - (call_stack.back().argc + 1),
                 data_stack.data() + call_stack.back().fp);
            
            peek(std::cout,
                 data_stack.data() + call_stack.back().fp,
                 data_stack.data() + data_stack.size()) << std::endl;
                 
            break;
            
          default:
            assert(false && "unknown opcode");
          };
        
          ++ip;
        };
    }

    

    std::ostream& operator<<(std::ostream& out, const machine& self) {
      return peek(out, self.data_stack.begin(), self.data_stack.end());
    }
  }  
  
}



