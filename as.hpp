#ifndef LINK_HPP
#define LINK_HPP

#include "vm.hpp"

#include <set>
#include <map>
#include <vector>

namespace as {

using label = const char*;

static label make_label(std::string name) {
  static std::set<std::string> table;
  return table.emplace(name).first->c_str();
}

struct line {
  union {
    struct {
      vm::instr op;
      label addr;
    } instr;

    vm::word data;
    label addr;
  } value;

  enum { INSTR, DATA, ADDR } kind;

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
      if(!info.second)
        throw std::runtime_error("duplicate label");
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
      if(at == table.end())
        throw std::runtime_error("unknown label");
      const vm::word offset = at->second - &it;
      result.emplace_back(offset);
      break;
    }
    }
  }

  return result;
}

} // namespace as

#endif
