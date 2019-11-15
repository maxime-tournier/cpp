#include "vm.hpp"

#include <vector>
#include <set>
#include <map>

#include <iostream>


using label = const char*;

static label make_label(std::string name) {
  static std::set<std::string> table;
  return table.emplace(name).first->c_str();
}


struct line {
  union  {
    struct {
      vm::instr op;
      label addr;
    } instr;
  
    vm::word data;
    label addr;
  } value;

  enum {INSTR, DATA, ADDR} kind;

  line(vm::instr op) {
    kind = INSTR;
    value.instr = {op, nullptr};
  }

  line(label addr, vm::instr op) {
    kind = INSTR;
    value.instr = {op, addr};
  }

  
  line(vm::word data) {
    kind = DATA;
    value.data = data;
  }

  line(label addr) {
    kind = ADDR;
    value.addr = addr;
  }
};

static std::vector<vm::code> link(const std::vector<line>& listing) {
  std::map<label, const line*> table;
  
  for(const line& it: listing) {
    // build address table
    if(it.kind == line::INSTR && it.value.instr.addr) {
      const auto info = table.emplace(it.value.instr.addr, &it);
      if(!info.second) throw std::runtime_error("duplicate label");
    }
  }
  
  std::vector<vm::code> result;
  
  for(const line& it: listing) {
    switch(it.kind) {
    case line::INSTR:
      result.emplace_back(it.value.instr.op);
      break;
    case line::DATA:
      result.emplace_back(it.value.data);
      break;
    case line::ADDR: {
      const auto at = table.find(it.value.addr);
      if(at == table.end()) throw std::runtime_error("unknown label");
      const vm::word offset = at->second - &it;
      result.emplace_back(offset);
      break;
    }
    }
  }

  return result;
}


using namespace vm;




static std::vector<line> test = 
{
 push, word(42),
 ret
};


static std::vector<line> id = 
{
 push, word(42),
 call, label("id"), word(1),

 
 {label("id"), load},
 word(-1),
 ret
};



static std::vector<line> fact = 
{
 push, word(3),
 call, label("fact"), word(1),

 // load arg
 {label("fact"), load},
 word(-1),

 // compare
 push,
 word(0),

 cmp<eq>,
 jmp, label("n=0"),

 // load n
 load, word(-1),

 // compute n - 1
 push, word(1),
 load, word(-1),
 op<sub>,

 // compute fact n - 1
 call, label("fact"), word(1),

 // multiply
 op<mul>,
 
 jmp, label("rest"),
 {label("n=0"), push},
 word(1),

 {label("rest"), ret}
};


  
int main(int, char**) {
  static constexpr std::size_t size = 1024;
  static vm::word stack[size];

  const auto prog = link(fact);
  std::cout << vm::eval(prog.data(), stack) << std::endl;
  
  return 0;
}
