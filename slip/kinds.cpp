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

    // substitute all kind variables in a given kind
    struct substitute_kind_visitor {
      using value_type = kind;
      using uf_type = union_find<kind>;
      
      kind operator()(const constant& self, const uf_type& uf) const { 
        return self;
      }
      
      kind operator()(const variable& self, const uf_type& uf) const {
        return uf.find(self);
      }

      kind operator()(const constructor& self, const uf_type& uf) const {
        return map(self, [&uf](const kind& k) {
            return k.apply(substitute_kind_visitor(), uf);
          });
      }
      
    };

    kind substitute(const kind& k, const union_find<kind>& uf) {
      return uf.find(k).apply(substitute_kind_visitor(), uf);
    }
    

    
    
  }
}
