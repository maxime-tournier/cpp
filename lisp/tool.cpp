#include "tool.hpp"

#include <iostream>

namespace lisp {


  static value list_ctor(const value* first, const value* last) {
    return make_list<value>(first, last);
  }

  static value cons(const value* first, const value* last) {
    return first[0] >>= first[1].get<value::list>();
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

    void operator()(const unit& self, std::ostream& out) const {
      out << "unit";
    }


    void operator()(const ref<std::string>& self, std::ostream& out) const {
      out << '"' << *self << '"';
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

    void operator()(const unit& self, std::ostream& out) const {
      out << "unit";
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
    return unit();
  };

  static symbol t("t");
  

  ref<environment> std_env(int argc, char** argv) {
    
    ref<environment> ctx = make_ref<environment>();
    
    (*ctx)
      ("+", wrap([](integer x, integer y) -> integer { return x + y; }))
      ("-", wrap([](integer x, integer y) -> integer { return x - y; }))
      ("*", wrap([](integer x, integer y) -> integer { return x * y; }))
      ("/", wrap([](integer x, integer y) -> integer { return x / y; }))
      ("%", wrap([](integer x, integer y) -> integer { return x % y; }))        
      
      ("list", list_ctor)
      ("cons", cons)      

      ("repr", ostream<repr_visitor>)
      ("print", ostream<print_visitor>)
      
      ("=", wrap([](integer x, integer y) -> boolean { return x == y; }))
      ("eq?", +[](const value* first, const value* last) -> value {
        argument_error::check(last - first, 2); 
        return first[0] == first[1];
      })
      ;

    return ctx;
  }
  
  
}
