#include "symbol.hpp"

namespace lisp {

  symbol::symbol(const std::string& value) 
    : iterator(table().insert(value).first) { }  
  

  symbol::symbol(const char* value) 
    : iterator(table().insert(value).first) { }  
  

  symbol::table_type& symbol::table() {
    static table_type instance;
    return instance;
  }
  

  std::ostream& operator<<(std::ostream& out, const symbol& s) {
    return out << s.name();
  }
  
}
