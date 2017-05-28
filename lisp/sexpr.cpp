#include <ostream>
#include "sexpr.hpp"
#include <iostream>

namespace lisp {



  struct ostream_visitor {

    void operator()(const sexpr::list& self, std::ostream& out) const {
      out << '(' << self << ')';
    }

    void operator()(const symbol& self, std::ostream& out) const {
      out << self.name();
    }

    void operator()(const ref<string>& self, std::ostream& out) const {
      out << '"' << *self << '"';
    }

    
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }
    
  };

  std::ostream& operator<<(std::ostream& out, const sexpr& self) {
    self.apply( ostream_visitor(), out );
    return out;
  }

  

}
