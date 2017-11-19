#include "kinds.hpp"

#include <ostream>
#include <sstream>

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
        const kind res = uf.find(self);
        if(res == self) return res;
        return res.apply( substitute_kind_visitor(), uf);
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

    // kind unification
    template<class UF>
    static void unify(UF& uf, kind lhs, kind rhs) {
      lhs = uf.find(lhs);
      rhs = uf.find(rhs);

      if(lhs.is<constructor>() && rhs.is<constructor>()) {
        unify(uf, lhs.get<constructor>().from, rhs.get<constructor>().from);
        unify(uf, lhs.get<constructor>().to, rhs.get<constructor>().to);
        return;
      }

      
      if(lhs.is<variable>() || rhs.is<variable>()) {
        const variable& var =  lhs.is<variable>() ?
          lhs.get<variable>() : rhs.get<variable>();
        const kind& other =  lhs.is<variable>() ? rhs : lhs;

        // note: other becomes representant
        uf.link(var, other);

        return;
      }

      if(lhs != rhs) {
        std::stringstream ss;
        ss << lhs << " vs. " << rhs;
        throw kind_error(ss.str());
      }
        
    }

    struct unbound_variable : kind_error {
      unbound_variable(symbol id) 
        : kind_error("unbound variable \"" + id.str() + "\"") { }
    };
    
    
    struct infer_visitor {
      using value_type = kind;
      using uf_type = union_find<kind>;
      using db_type = environment;
      
      template<class T>
      kind operator()(const T& self, const db_type& db, uf_type& uf) const {
        if(auto* k = db.find(self.name)) {
          return *k;
        }

        throw unbound_variable(self.name);
      }

      kind operator()(const ast::type_application& self, const db_type& db, uf_type& uf) const {

        const kind func = self.ctor.apply( infer_visitor(), db, uf);
        const kind result = variable();

        const kind call =
          foldr(result, self.args, [&](const ast::type& lhs, const kind& rhs) {
              const kind result = lhs.apply(infer_visitor(), db, uf) >>= rhs;
              return result;
            });
        
        unify(uf, func, call);
        
        return result;
      }

    };


    

    kind infer(union_find<kind>& uf, const ast::type& node, const environment& env) {
      return node.apply(infer_visitor(), env, uf);
    }
    
    
  }
}
