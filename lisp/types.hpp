#ifndef LISP_TYPES_HPP
#define LISP_TYPES_HPP

#include <vector>
#include <set>
#include <algorithm>

#include "context.hpp"
#include "sexpr.hpp"

#include "../variant.hpp"
#include "../ref.hpp"

namespace lisp {

  namespace types {
  
    // monotypes
    struct mono;

    
    struct constant {
      symbol name;
      constant(symbol name) : name(name) { }

      bool operator==(const constant& other) const { return name == other.name; }
      bool operator<(const constant& other) const { return name < other.name; }      
    };

    extern const constant unit_type, boolean_type, integer_type, real_type, string_type;

    
    struct variable {
      const std::size_t depth;
      variable(std::size_t depth = 0) : depth(depth) { }
      
      static ref<variable> fresh(std::size_t depth = 0) {
        return make_ref<variable>(depth);
      }
    };

  
    class constructor {
      using table_type = std::map<symbol, std::size_t>;
      static table_type table;

      table_type::iterator iterator;
    public:
      
      constructor(symbol name, std::size_t argc) {
        auto err = table.insert( std::make_pair(name, argc) );
        if(!err.second) throw error("type constructor already defined");
        iterator = err.first;
      }

      constructor(symbol name) {
        auto it = table.find(name);
        if(it == table.end()) throw error("unknown type constructor");
        iterator = it;
      }

      symbol name() const { return iterator->first; }      
      std::size_t argc() const { return iterator->second; }

      bool operator==(const constructor& other) const {
        return iterator == other.iterator;
      }

      bool operator<(const constructor& other) const {
        // mmhhh
        return &*iterator < &*other.iterator;
      }

      
    };

    extern const constructor func_ctor;
    
  
    struct application {
      constructor ctor;

      // TODO use a list instead?
      using args_type = std::vector<mono>;
      args_type args;

      application(constructor ctor, const args_type& args)
        : ctor(ctor), args(args) {
        if(args.size() != ctor.argc()) {
          throw error("constructor argc error");
        }
      }

      bool operator==(const application& other) const {
        return ctor == other.ctor && args == other.args;
      }

      bool operator<(const application& other) const {
        return ctor < other.ctor || (ctor == other.ctor && args < other.args);
      }

      
    };

    template<class F>
    static application map(const application& self, const F& f) {
      application::args_type args; args.reserve(self.args.size());

      std::transform(self.args.begin(), self.args.end(), std::back_inserter(args), f);

      return application(self.ctor, args);
    }
    

    struct scheme;

    
    using vars_type = std::set< ref<variable > >;

    
    struct mono : variant<constant, ref<variable>, application> {
      using mono::variant::variant;

      vars_type vars() const;
      scheme generalize(std::size_t depth = 0) const;

    };

    application operator>>=(const mono& lhs, const mono& rhs);

    

    // std::ostream& operator<<(std::ostream& out, const mono& self);

    
    struct scheme {
      vars_type vars;
      
      mono body;

      explicit scheme(const mono& body) : body(body) { }
      
      mono instantiate(std::size_t depth) const;
    };

    std::ostream& operator<<(std::ostream& out, const scheme& self);

    
    struct context : lisp::context< context, scheme > {
      const std::size_t depth;
      
      context(const ref<context>& parent = {})
        : base::base(parent),
        depth( parent ? parent->depth + 1 : 0 ) {
        
      }


      context& def(symbol id, mono t) {
        // TODO nice?
        locals.emplace(id, t.generalize(depth));
        return *this;
      }
      
    };


    scheme check(const ref<context>& ctx, const sexpr& e);

    
    template<class T>
    static bool operator!=(const T& lhs, const T& rhs) {
      return !(lhs == rhs);
    }
     
  }
}


#endif
