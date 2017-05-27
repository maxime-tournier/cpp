#ifndef EVAL_HPP
#define EVAL_HPP

#include "value.hpp"
#include <map>

namespace lisp {

  struct unbound_variable : error {
    unbound_variable(const symbol& s);
    
  };

  
  struct context {
    
    ref<context> parent;
    context(const ref<context>& parent = {}) 
      : parent(parent) { }
    
    using locals_type = std::map< symbol, value >;
    locals_type locals;

    using macros_type = std::map< symbol, value >;
    macros_type macros;
    
    value& find(const symbol& name) {
      auto it = locals.find(name);
      if(it != locals.end()) return it->second;
      if(parent) return parent->find(name);
      throw unbound_variable(name);
    }

    context& operator()(const symbol& name, const value& expr) {
      locals.emplace( std::make_pair(name, expr) );
      return *this;
    }
    
  };


  template<class Iterator>
  static ref<context> extend(const ref<context>& parent, Iterator first, Iterator last) {
    ref<context> res = make_ref<context>(parent);
    res->locals.insert(first, last);
    
    return res;
  }


  value eval(const ref<context>& ctx, const value& expr);
  value apply(const value& app, const value* first, const value* last);
  
}

#endif
