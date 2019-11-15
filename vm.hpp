#include <cstdint>
#include <cassert>

namespace vm {

  
using word = std::int64_t;
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
};

// fetch next instruction as data
static word fetch_data(frame* caller) { return (++caller->ip)->data; }

// fetch next instruction as code pointer 
static const code* fetch_code(frame* caller) {
  const word offset = fetch_data(caller);
  return caller->ip + offset;
}

// next []
static void next(frame* caller) { ++caller->ip; }

// jump [offset]
static void jmp(frame* caller) {
  caller->ip = fetch_code(caller);
}

// jnz [offset]
static void jnz(frame* caller) {
  if(*(--caller->sp)) {
    jmp(caller);
  } else {
    next(caller);
  }
}


// comparisons
  using binary_predicate = bool (*) (word, word);
template<binary_predicate pred>
static void cmp(frame* caller) {
  const word rhs = *(--caller->sp);
  const word lhs = *(--caller->sp);
  
  *caller->sp++ = pred(lhs, rhs);
  next(caller);
}

static bool eq(word lhs, word rhs) { return lhs == rhs; }
static bool ne(word lhs, word rhs) { return lhs != rhs; }
static bool le(word lhs, word rhs) { return lhs <= rhs; }
static bool lt(word lhs, word rhs) { return lhs < rhs; }
static bool ge(word lhs, word rhs) { return lhs >= rhs; }
static bool gt(word lhs, word rhs) { return lhs > rhs; }


using binary_operation = word (*) (word, word);
template<binary_operation binop>
static void op(frame* caller) {
  const word rhs = *(--caller->sp);
  const word lhs = *(--caller->sp);
  
  *caller->sp++ = binop(lhs, rhs);
  next(caller);
}

static word add(word lhs, word rhs) { return lhs + rhs; }
static word sub(word lhs, word rhs) { return lhs - rhs; }
static word mul(word lhs, word rhs) { return lhs * rhs; }
  
// 
  

// run []
static void run(frame* callee) {
  while(callee->ip->op) {
    (*callee->ip->op)(callee);
  }
}

// call [offset, argc]
static void call(frame* caller) {
  frame callee{fetch_code(caller), caller->sp, caller->sp};
  const word argc = fetch_data(caller);
  assert(argc >= 0);
  
  run(&callee);
  
  // replace args with result
  caller->sp[-argc] = *caller->sp;
  caller->sp -= argc;
  ++caller->sp;
  
  next(caller);
}

// push [word]
static void push(frame* caller) {
  *caller->sp++ = fetch_data(caller);
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

// load [offset]
static void load(frame* caller) {
  const word index = fetch_data(caller);
  *caller->sp++ = caller->fp[index];
  next(caller);
}

static const instr ret = {0};


// toplevel eval
static word eval(const code* prog, word* stack) {
  frame init{prog, stack, stack};
  run(&init);
  return stack[0];
}


} // namespace vm
