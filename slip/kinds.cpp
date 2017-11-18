#include "kinds.hpp"

#include <ostream>


namespace slip {
  namespace kinds {
    
    bool constant::operator==(const constant& other) const { 
      return name == other.name;
    }

    bool constant::operator<(const constant& other) const { 
      return name < other.name;
    }

    
    bool constructor::operator==(const constructor& other) const {
      return from == other.from && to == other.to;
    }

    bool constructor::operator<(const constructor& other) const {
      return from < other.from || (from == other.from && to < other.to);
    }

    
    bool variable::operator==(const variable& other) const {
      return index == other.index;
    }

    bool variable::operator<(const variable& other) const {
      return index < other.index;
    }
    
    
    kind operator>>=(const kind& lhs, const kind& rhs) {
      return constructor{lhs, rhs};
    }

    static std::size_t next() {
      static std::size_t count = 0;
      return count++;
    }
    
    variable::variable() : index( next() )  { }


    struct ostream_visitor {
      void operator()(const constant& self, std::ostream& out) const {
        out << self.name;
      }

      void operator()(const variable& self, std::ostream& out) const {
        out << "?";
      }

      void operator()(const constructor& self, std::ostream& out) const {
        out << self.from << " -> " << self.to;
      }
      
    };
    
    std::ostream& operator<<(std::ostream& out, const kind& self) {
      self.apply( ostream_visitor(), out);
      return out;
    }
    
  }
}
