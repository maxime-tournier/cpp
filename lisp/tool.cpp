#include "tool.hpp"

#include <iostream>

namespace lisp {


  static value list_ctor(const value* first, const value* last) {
    list res;
    list* curr = &res;
    
    for(const value* it = first; it != last; ++it) {
      *curr = make_ref<cell>(*it, nil);
      curr = &(*curr)->tail;
    }
                                                 
    return res;
  }

  struct eq_visitor {

    template<class T>
    void operator()(const T& self, const value& other, bool& result) const {
      result = self == other.template get<T>();
    }
    
  };


  ref<context> std_env(int argc, char** argv) {
    
    ref<context> ctx = make_ref<context>();
    
    (*ctx)
      ("add", +[](const value* first, const value* last) -> value {
      integer res = 0;
      while(first != last) {
        res += (first++)->cast<integer>();
      };
      return res;
    })
    ("sub", wrap([](integer x, integer y) -> integer { return x - y; }))
    ("mul", wrap([](integer x, integer y) -> integer { return x * y; }))
    ("div", wrap([](integer x, integer y) -> integer { return x / y; }))
    ("mod", wrap([](integer x, integer y) -> integer { return x % y; }))        
    
    ("list", list_ctor)
    ("print", +[](const value* first, const value* last) -> value {
      bool start = true;
      for(const value* it = first; it != last; ++it) {
        if(start) start = false;
        else std::cout << ' ';
        std::cout << *it;
      }
      std::cout << std::endl;
      return nil;
    })
    ("eq", +[](const value* first, const value* last) -> value {
      if(last - first < 2) throw error("argc");
      if(first[0].type() != first[1].type()) return nil;
      bool res;
      first[0].apply(eq_visitor(), first[1], res);
      if(!res) return nil;

      static symbol t("t");
      return t;
    })
    ;


    return ctx;
  }
  
  
}
