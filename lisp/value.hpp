#ifndef VALUE_HPP
#define VALUE_HPP

#include "../variant.hpp"
#include "../ref.hpp"

#include "error.hpp"

#include <string>
#include <ostream>
#include <set>
#include <vector>

namespace lisp {
 
  struct value;
  
  struct cell;
  using list = ref<cell>;

  struct context;
  struct lambda;
  
  using string = std::string;
  using integer = long;
  using real = double; 
 
  // TODO pass ctx ?
  using builtin = value (*)(const value* first, const value* last);

  // symbols
  class symbol {
    using table_type = std::set<string>;
    static table_type& table();
    
    using iterator_type = table_type::iterator;
    iterator_type iterator;
  public:

    // TODO do we want this ?
    symbol() : iterator(table().end()) { }
    
    symbol(const string& value) 
      : iterator(table().insert(value).first) { }  

    symbol(const char* value) 
      : iterator(table().insert(value).first) { }  


    bool operator==(const symbol& other) const {
      return iterator == other.iterator;
    }
    
    const std::string& name() const { return *iterator; }

    bool operator<(const symbol& other) const {
      return &(*iterator) < &(*other.iterator);
    }
    
  };

  struct value : variant<list, integer, real, symbol, ref<string>,
                         ref<lambda>, builtin > {
    using value::variant::variant;

    value(value::variant&& other)
      : value::variant( std::move(other))  { }

    value(const value::variant& other)
      : value::variant(other) { }

    
    list operator>>=(list tail) const {
      return make_ref<cell>(*this, tail);
    }
 
    struct ostream;

    // nil check
    explicit operator bool() const {
      return !is<list>() || get<list>();
    }

  }; 

  // lists
  struct cell {

    value head; 
    list tail;

    cell(const value& head,
         const list& tail)
      : head(head),
        tail(tail) { 

    }
    
    struct iterator {
      cell* ptr;

      value& operator*() const { return ptr->head; }
      bool operator!=(iterator other) const { return ptr != other.ptr; }
      iterator& operator++() { ptr = ptr->tail.get(); return *this; }
    };

  };
  
  static inline cell::iterator begin(const list& self) { return { self.get() }; }
  static inline cell::iterator end(const list& self) { return { nullptr }; }


  static const list nil;  
  
  template<class Iterator>
  static list make_list(Iterator first, Iterator last) {
    list res;
    list* curr = &res;
    
    for(Iterator it = first; it != last; ++it) {
      *curr = make_ref<cell>(*it, nil);
      curr = &(*curr)->tail;
    }
    
    return res;
  }
 

  struct empty_list : error {
    empty_list() : error("empty list") { } 
  };

  
  static inline const list& unpack(const list& from) { return from; }
  
  template<class H, class ... Args>
  static inline const list& unpack(const list& from, H* to, Args*... args) {
    if(!from) throw empty_list();
    *to = from->head.cast<H>();
    return unpack(from->tail, args...);
  }


  static inline value& head(const list& x) {
    if(!x) throw empty_list();
    return x->head;
  }

  static inline const list& tail(const list& x) {
    if(!x) throw empty_list();
    return x->tail;
  }
  


  // lambda
  struct lambda {

    ref<context> ctx;
    using args_type = std::vector<symbol>;
    args_type args;
    value body;


    lambda(const ref<context>& ctx,
           args_type&& args,
           const value& body);
    
    
  };

  
  // ostream
  std::ostream& operator<<(std::ostream& out, const value& self);  
  

}



#endif
