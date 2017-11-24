#include <ostream>
#include "sexpr.hpp"
#include <iostream>

namespace slip {



  struct ostream_visitor {

    void operator()(const sexpr::list& self, std::ostream& out) const {
      out << '(' << self << ')';
    }

    void operator()(const symbol& self, std::ostream& out) const {
      out << self.str();
    }

    void operator()(const ref<string>& self, std::ostream& out) const {
      out << '"' << *self << '"';
    }

    void operator()(const boolean& self, std::ostream& out) const {
      if(self) out << "true";
      else out << "false";
    }


    void operator()(const unit& , std::ostream& out) const {
      out << "unit";
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
