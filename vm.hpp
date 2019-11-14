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
  code* ip;
  word* fp;
  word* sp;
};

// fetch next instruction as data
static word fetch_data(frame* caller) { return (++caller->ip)->data; }

// fetch next instruction as code pointer  
static code* fetch_code(frame* caller) {
  // note: offset is relative to the current instruction
  code* const base = caller->ip;
  const word offset = fetch_data(caller);

  return base + offset;
}

// next []
static void next(frame* caller) { ++caller->ip; }

// jump [offset]
static void jump(frame* caller) {
  // note: offset is relative to the jump instruction
  caller->ip = fetch_code(caller);
}


// run []
static void run(frame* callee) {
  while(callee->ip->op) {
    (*callee->ip->op)(callee);
  }
}

// call [offset, argc]
static void call(frame* caller) {
  // note: offset is relative to the call instruction
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
static word eval(code* prog, word* stack) {
  frame init{prog, stack, stack};
  run(&init);
  return stack[0];
}


} // namespace vm
