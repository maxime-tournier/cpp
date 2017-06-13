#ifndef LISP_TYPES_HPP
#define LISP_TYPES_HPP

#include <vector>
#include <set>

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
    };

    
    struct variable {
      const std::size_t depth;
      variable(std::size_t depth) : depth(depth) { }
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
      
    };

  
    struct application {
      constructor ctor;
      std::vector<mono> args;

      application(constructor ctor, const std::vector<mono>& args)
        : ctor(ctor), args(args) {
        if(args.size() != ctor.argc()) {
          throw error("constructor argc error");
        }
      }
    };


    struct scheme;

    
    using vars_type = std::set< ref<variable > >;

    
    struct mono : variant<constant, ref<variable>, application> {
      using mono::variant::variant;

      vars_type vars() const;
      scheme generalize(std::size_t depth = 0) const;
    };


    std::ostream& operator<<(std::ostream& out, const mono& self);


    struct scheme {
      vars_type vars;
      
      mono body;

      scheme(const mono& body) : body(body) { }
      
      mono instantiate(std::size_t depth) const;
    };

    std::ostream& operator<<(std::ostream& out, const scheme& self);

    
    struct context : lisp::context< context, scheme > {
      const std::size_t depth;
      
      context(const ref<context>& parent = {})
        : base::base(parent),
        depth( parent ? parent->depth + 1 : 0 ) {
        
      }
      
      ref<variable> fresh() const {
        return make_ref<variable>(depth);
      }
    };


    scheme check(const ref<context>& ctx, const sexpr& e);
    
  }
}


#endif
