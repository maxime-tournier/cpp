#include "symbol.hpp"
#include <ostream>

namespace slip {

  symbol::symbol(const std::string& value) 
    : iterator(table().insert(value).first) {
    if(value.empty()) throw std::logic_error("empty symbol");
  }  
  

  symbol::symbol(const char* value) 
    : iterator(table().insert(value).first) {
    if(!value || ! *value) throw std::logic_error("empty symbol");
  }  
  

  symbol::table_type& symbol::table() {
    static table_type instance;
    return instance;
  }
  

  std::ostream& operator<<(std::ostream& out, const symbol& s) {
    return out << s.name();
  }
  
}
