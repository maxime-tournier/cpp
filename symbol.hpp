#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <set>
#include <string>
#include <ostream>

struct symbol {
  const char* repr;
  symbol(const char* repr) {
    static std::set<std::string> table;
    this->repr = table.emplace(repr).first->c_str();
  }

  friend std::ostream& operator<<(std::ostream& out, symbol self) {
    return out << self.repr;
  }

  bool operator<(symbol other) const { return repr < other.repr; }
};

#endif
