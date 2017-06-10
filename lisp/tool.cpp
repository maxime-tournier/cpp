#include "tool.hpp"

#include <iostream>

namespace lisp {


  static value list_ctor(const value* first, const value* last) {
    return make_list<value>(first, last);
  }

  static value append(const value* first, const value* last) {
    if(last - first != 2) throw error("argc");
    
    const value& head = *first++;
    try{
      const value::list& tail = first->cast<value::list>();
      return head >>= tail;
    } catch( value::bad_cast& e ) {
      throw type_error("list expected");
    }
  }

  
  struct eq_visitor {

    template<class T>
    void operator()(const T& self, const value& other, bool& result) const {
      result = self == other.template get<T>();
    }
    
  };

  struct repr_visitor {
    
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }

    void operator()(const ref<std::string>& self, std::ostream& out) const {
      out << *self;
    }

    
    void operator()(const symbol& self, std::ostream& out) const {
      out << '\'' << self.name();
    }
    
  };

  struct print_visitor {
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }

    void operator()(const ref<string>& self, std::ostream& out) const {
      out << *self;
    }
    
  };


  template<class Visitor>
  static value ostream(const value* first, const value* last) {
    bool start = true;
    for(const value* it = first; it != last; ++it) {
      if(start) start = false;
      else std::cout << ' ';
      
      it->apply(Visitor(), std::cout);
    }
    std::cout << std::endl;
    return value::list();
  };

  static symbol t("t");
  

  ref<context> std_env(int argc, char** argv) {
    
    ref<context> ctx = make_ref<context>();
    
    (*ctx)
      ("+", wrap([](integer x, integer y) -> integer { return x + y; }))
      ("-", wrap([](integer x, integer y) -> integer { return x - y; }))
      ("*", wrap([](integer x, integer y) -> integer { return x * y; }))
      ("/", wrap([](integer x, integer y) -> integer { return x / y; }))
      ("%", wrap([](integer x, integer y) -> integer { return x % y; }))        
      
      ("list", list_ctor)
      ("append", append)      

      ("repr", ostream<repr_visitor>)
      ("print", ostream<print_visitor>)
      
      ("=", +[](const value* first, const value* last) -> value {
        if(last - first != 2) throw error("argc");
        if(first[0].type() != first[1].type()) return value::list();
        bool res;
        first[0].apply(eq_visitor(), first[1], res);
        if(!res) return value::list();
        
        return t;
      })
      ;


    return ctx;
  }
  
  
}
