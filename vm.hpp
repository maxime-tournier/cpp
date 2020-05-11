#ifndef VM_HPP
#define VM_HPP

#include <cstdint>
#include <cassert>

#include <iostream>

namespace vm {

struct closure;

using integer = std::int64_t;
  
union word {
  integer value;
  const closure* func;

  word(integer value): value(value) { }
  word(const closure* func): func(func) { }
  word() = default;
};
  
struct frame;

using instr = void (*)(frame*);


union code {
  instr op;
  word data;
  
  code(instr op): op(op) { }
  code(word data): data(data) { }
};


struct frame {
  const code* ip;
  word* fp;
  word* sp;

  friend std::ostream& operator<<(std::ostream& out, const frame& self) {
    for(const word* it = self.fp; it != self.sp; ++it) {
      out << it - self.fp << "\t" << it->value << '\n';
    }
    return out;
  }
};

// fetch next instruction as data
static integer fetch_lit(frame* caller) { return (++caller->ip)->data.value; }

// fetch next instruction as code pointer 
static const code* fetch_addr(frame* caller) {
  const word offset = fetch_lit(caller);
  return caller->ip + offset.value;
}

// next []
static void next(frame* caller) { ++caller->ip; }

// jump [offset]
static void jmp(frame* caller) {
  caller->ip = fetch_addr(caller);
}

// jnz [offset]
static void jnz(frame* caller) {
  if((--caller->sp)->value) {
    jmp(caller);
  } else {
    next(caller);
    next(caller);    
  }
}


// comparisons
using binary_predicate = bool (*)(integer, integer);
template<binary_predicate pred>
static void cmp(frame* caller) {
  const auto rhs = (--caller->sp)->value;
  const auto lhs = (--caller->sp)->value;
  
  *caller->sp++ = pred(lhs, rhs);
  next(caller);
}

static bool eq(integer lhs, integer rhs) { return lhs == rhs; }
static bool ne(integer lhs, integer rhs) { return lhs != rhs; }
static bool le(integer lhs, integer rhs) { return lhs <= rhs; }
static bool lt(integer lhs, integer rhs) { return lhs < rhs; }
static bool ge(integer lhs, integer rhs) { return lhs >= rhs; }
static bool gt(integer lhs, integer rhs) { return lhs > rhs; }


// binary ops
using binary_operation = integer (*) (integer, integer);
template<binary_operation binop>
static void op(frame* caller) {
  const integer lhs = (--caller->sp)->value;
  const integer rhs = (--caller->sp)->value;
  
  *caller->sp++ = binop(lhs, rhs);
  next(caller);
}

static integer add(integer lhs, integer rhs) { return lhs + rhs; }
static integer sub(integer lhs, integer rhs) { return lhs - rhs; }
static integer mul(integer lhs, integer rhs) { return lhs * rhs; }
static integer div(integer lhs, integer rhs) { return lhs / rhs; }
static integer mod(integer lhs, integer rhs) { return lhs % rhs; }    
  
  

// run []
static void run(frame* callee) {
  while(callee->ip->op) {
    (*callee->ip->op)(callee);
  }
}


static void call(frame* caller, std::size_t argc) {
  const code* addr = fetch_addr(caller);
  frame callee{addr, caller->sp, caller->sp};
  
  run(&callee);
  
  // replace args with result
  caller->sp[-argc] = callee.sp[-1];
  caller->sp += 1 - argc;
  
  next(caller);
}

// call [argc, addr]
static void call(frame* caller) {
  const integer argc = fetch_lit(caller);
  assert(argc >= 0);

  call(caller, argc);
}

template<std::size_t argc>
static void call(frame* caller) {
  call(caller, argc);
}

  
// push [word]
static void push(frame* caller) {
  *caller->sp++ = fetch_lit(caller);
  next(caller);
}

// pop []
static void pop(frame* caller) {
  --caller->sp;
  next(caller);
}

// dup []
static void dup(frame* caller) {
  caller->sp[0] = caller->sp[-1];
  ++caller->sp;
  next(caller);
}

static void load(frame* caller, integer index) {
  *caller->sp++ = caller->fp[index];
  next(caller);
}
  
// load [offset]
static void load(frame* caller) {
  const integer index = fetch_lit(caller);
  load(caller, index);
}

// load []
template<integer index>
static void load(frame* caller) {
  load(caller, index);
}

  
static const instr ret = {0};


// toplevel eval
static word eval(const code* prog, word* stack) {
  frame init{prog, stack, stack};
  run(&init);
  return stack[0];
}

template<instr op=next>
static void debug(frame* caller) {
  op(caller);
  std::clog << *caller << std::endl;
}

////////////////////////////////////////////////////////////////////////////////  
// closure business
struct gc {
  gc** prev;
  gc* next;
  
  static gc* first;
  
  gc(): prev(&first), next(first) {
    first->prev = &next;
    first = this;
  }
  
  virtual ~gc() {
    next->prev = prev;      
    *prev = next;
  }

};
  
struct closure: gc {
  const code* impl;
  const std::size_t argc;
  const word* data;

  closure(const code* impl,
          std::size_t argc,          
          const word* data):
    impl(impl),
    argc(argc),
    data(data) { }
  
  ~closure() {
    delete [] data;
  }
};



static void callc(frame* caller, std::size_t argc) {
  const closure* func = caller->sp[-1].func;
  frame callee{func->impl, caller->sp, caller->sp};
  
  run(&callee);
  
  // replace args with result
  caller->sp[-argc] = callee.sp[-1];
  caller->sp += 1 - argc;
  
  next(caller);
}

// callc [argc]
static void callc(frame* caller) {
  // args pushed in reverse, then closure
  const integer argc = fetch_lit(caller);
  assert(argc >= 0);

  callc(caller, argc);
}

template<std::size_t argc>
static void callc(frame* caller) {
  callc(caller, argc);
}
  
  

static void loadc(frame* caller, std::size_t index) {
  const closure* func = caller->fp[-1].func;
  *(caller->sp++) = func->data[index];

  next(caller);  
}

// loadc [index]
static void loadc(frame* caller) {
  const integer index = fetch_lit(caller);
  assert(index >= 0);

  loadc(caller, index);
}

template<std::size_t index>  
static void loadc(frame* caller) {  
  loadc(caller, index);
}


static void makec(frame* caller, std::size_t argc, std::size_t cap) {
  const code* addr = fetch_addr(caller);
  
  word* data = new word[cap];
  for(std::size_t i = 0; i < cap; ++i) {
    data[i] = *(--caller->sp);
  }

  *caller->sp++ = new closure(addr, argc, data);
  next(caller);
}

// makec [argc, cap, addr]  
static void makec(frame* caller) {
  const integer argc = fetch_lit(caller);
  assert(argc >= 0);

  const integer cap = fetch_lit(caller);
  assert(cap >= 0);

  makec(caller, argc, cap);
}

  
template<std::size_t argc, std::size_t cap>
static void makec(frame* caller) {
  makec(caller, argc, cap);
}

  
} // namespace vm



#endif
