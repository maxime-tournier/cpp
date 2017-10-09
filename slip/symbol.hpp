#ifndef LISP_SYMBOL_HPP
#define LISP_SYMBOL_HPP

#include <set>
#include <string>

namespace slip {

  class symbol {
    using table_type = std::set<std::string>;
    static table_type& table();
    
    using iterator_type = table_type::iterator;
    iterator_type iterator;
  public:

    // TODO do we want this ?
    symbol() : iterator(table().end()) { }
    
    symbol(const std::string& value);
    symbol(const char* value);    
    
    inline bool operator==(const symbol& other) const {
      return iterator == other.iterator;
    }
    
    inline const std::string& str() const { return *iterator; }

    inline bool operator<(const symbol& other) const {
      return &(*iterator) < &(*other.iterator);
    }
    
  };


  std::ostream& operator<<(std::ostream& out, const symbol& s);

}

#endif
