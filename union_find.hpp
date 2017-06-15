#ifndef UNION_FIND_HPP
#define UNION_FIND_HPP

#include <map>

template<class T>
class union_find {
  using parent_type = std::map<T, T>;
  mutable parent_type parent;
public:

  const T& find(const T& value) const {

    auto it = parent.find(value);
    if(it == parent.end() ) {
      return value;
    }
    
    const T& res = find(it->second);
    
    // path compression
    if(res != it->second) {
      parent.emplace( std::make_pair(value, res) );
    }
    
    return res;
  }

  void link(const T& from, const T& to) {
    if(from != to) parent.emplace( std::make_pair(from, to) );
  }
  
};

#endif
