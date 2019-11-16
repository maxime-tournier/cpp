#include "vm.hpp"
#include "as.hpp"

#include <vector>
#include <set>
#include <map>

#include <iostream>




using namespace vm;
using namespace as;


static std::vector<line> test = 
{
 push, word(42),
 ret
};


static std::vector<line> id = 
{
 push, word(42),
 call, word(1), as::label("id"),

 
 {label("id"), load},
 word(-1),
 ret
};



static std::vector<as::line> fact = 
{
 push, word(10),
 call, word(1), label("fact"), 
 ret,
 
 // load arg
 {label("fact"), load},
 word(-1),

 // compare
 push,
 word(0),

 cmp<eq>,
 jnz, label("base"),
 
 // load n
 load, word(-1),

 // compute n - 1
 push, word(1),
 load, word(-1),
 op<sub>,

 // compute fact n - 1
 call, word(1), label("fact"), 

 // multiply
 op<mul>,
 
 jmp, label("rest"), 
 {label("base"), push},
 word(1),
 
 {label("rest"), ret}
};



static std::vector<line> fib = 
{
 push, word(35),
 call<1>, label("fib"), 
 ret,
 
 // load arg
 {label("fib"), load<-1>},
 
 // compare to 0
 push,
 word(0),

 cmp<eq>,
 
 jnz, label("zero"),

 // load arg
 load, word(-1),
 
 // compare to 1
 push,
 word(1), 

 cmp<eq>,
 
 jnz, label("one"),

 // compute n - 1
 push, word(1),
 load, word(-1),
 op<sub>,

 // compute fib n - 1
 call<1>, label("fib"), 

 // compute n - 2
 push, word(2),
 load, word(-1),
 op<sub>,

 // compute fib n - 2 
 call<1>, label("fib"), 
 
 // add 
 op<add>, 
 jmp, label("ret"),
 
 {label("zero"), push},
 word(0),
 jmp, label("ret"),

 {label("one"), push},
 word(1),
 jmp, label("ret"),
 
 {label("ret"), ret}
};


  
int main(int, char**) {
  static constexpr std::size_t size = 1024;
  static vm::word stack[size];

  const auto prog = as::link(fib);
  std::cout << vm::eval(prog.data(), stack) << std::endl;
  
  return 0;
}
