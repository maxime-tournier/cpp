#ifndef SLIP_KINDS_HPP
#define SLIP_KINDS_HPP

#include "../union_find.hpp"

#include "variant.hpp"

#include "symbol.hpp"
#include "error.hpp"
#include "context.hpp"

#include <iosfwd>

namespace slip {

  namespace kinds {

    struct kind;

    struct constant;
    struct constructor;
    struct variable;
    
    struct kind : variant<constant, constructor, variable> {
      using kind::variant::variant;
    };

    // constant kinds
    struct constant {
      const symbol name;
      
      bool operator==(const constant& other) const;
      bool operator<(const constant& other) const;      
    };

    // predefined constants
    const constant terms{"*"};
    const constant rows{"rows"};

    // the kind of type constructors
    struct constructor {
      const kind from, to;
      
      constructor(const constructor& ) = default;
      constructor(constructor&& ) = default;
      
      bool operator==(const constructor& other) const;
      bool operator<(const constructor& other) const;      
    };

    template<class F>
    static constructor map(const constructor& self, const F& f) {
      return { f(self.from), f(self.to) };
    }
    
    
    // convenience
    kind operator>>=(const kind& lhs, const kind& rhs);

    // a kind variable, used during kind inference
    struct variable {
      const std::size_t index;

      variable();
      
      bool operator==(const variable& other) const;
      bool operator<(const variable& other) const;      
    };

    std::ostream& operator<<(std::ostream& out, const kind& self);

    // kind substitution
    kind substitute(const kind& self, const union_find<kind>& uf);
  }
  
}


#endif
