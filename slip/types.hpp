#ifndef SLIP_TYPES_HPP
#define SLIP_TYPES_HPP

#include "variant.hpp"
#include "../ref.hpp"
#include "../union_find.hpp"

#include "symbol.hpp"
#include "error.hpp"
#include "context.hpp"
#include "kinds.hpp"

#include <vector>
#include <set>

#include <iostream>

namespace slip {

  namespace ast {
    struct toplevel;
  }

  
  namespace types {

    struct type;

    struct constant {
      symbol name;
      kinds::kind kind;
      
      constant(symbol name, kinds::kind kind = kinds::terms)
        : name(name),
          kind(kind) { }

      bool operator==(const constant& other) const {
        return name == other.name && kind == other.kind;
      }

      bool operator<(const constant& other) const {
        return name < other.name || (name == other.name && kind < other.kind);
      }
      
    };

    
    struct variable {
      std::size_t id;
      
      kinds::kind kind;
      std::size_t depth;        // TODO const?

      variable(const kinds::kind& kind, const std::size_t& depth)
        : kind(kind), depth(depth) {
        static std::size_t count = 0;
        id = count++;
      }

      bool operator==(const variable& other) const { return id == other.id; }
      bool operator<(const variable& other) const { return id < other.id; }
      
    };

    
    struct application;


    struct type : variant< constant, variable, application > {
      
      using type::variant::variant;
    
      kinds::kind kind() const;

      // apply a type constructor to a type.
      // TODO only for applications
      type operator()(const type& arg) const;
      
    };

    
    // application of a type constructor to a type
    struct application {
      type func;
      type arg;

      application(type func, type arg);
      application(const application& ) = default;

      bool operator==(const application& other) const;
      bool operator<(const application& other) const;      
      
      // used to remember "true" argcount in lambdas when currying
      std::size_t argc = 0;
    };

    template<class F>
    static application map(const application& self, const F& f) {
      return {f(self.func), f(self.arg)};
    }

    template<class F>
    static void iter(const application& self, const F& f) {
      f(self.func); f(self.arg);
    }

    

    // type schemes
    struct scheme {

      using forall_type = std::set< variable >;
      forall_type forall;
      
      const type body;

      explicit scheme(const type& body);

      using constraints_type = std::set<type>;
      constraints_type constraints;
    };

    std::ostream& operator<<(std::ostream& out, const scheme& self);
    
    

    // some type constructors
    extern const type func_ctor, io_ctor, list_ctor;
    
    // easily construct function types
    type operator>>=(const type& lhs, const type& rhs);

    
    // typing environment
    struct environment : context< environment, scheme > {
      std::size_t depth;

      environment( const ref<environment>& parent = {} )
        : environment::context(parent),
        depth( parent ? parent->depth + 1 : 0 ) {

      }

      

    };


    

    
    // datatype definitions
    struct datatypes : context< datatypes, type > {
      std::size_t depth;

      datatypes( const ref<datatypes>& parent = {} )
        : datatypes::context(parent),
        depth( parent ? parent->depth + 1 : 0 ) {
        
      }
      
    };

    // data constructors (FCP style)
    struct data_constructor {
      // which variables are quantified in source
      scheme::forall_type forall;
      
      // target -> source (must be rank1)      
      scheme unbox;

      // TODO check that forall is a subset of unbox.forall
    };
    
    // inference state monad
    class state {
    public:
      using env_type = environment;
      ref<env_type> env;

      using uf_type = union_find<type>;
      ref< uf_type > uf;

    public:
      
      // type constructors
      using ctor_type = datatypes;
      static ref<ctor_type> ctor;

      // data constructors (FCP style)
      using data_ctor_type = std::map<symbol, data_constructor>;
      static ref<data_ctor_type> data_ctor;
      
    public:
      state(ref<env_type> env = make_ref<env_type>(),
            ref<uf_type> uf = make_ref<uf_type>());
      
      scheme generalize(const type& t, const scheme::constraints_type& constraints = {}) const;

      struct instantiate_type {
        const struct type type;
        const scheme::constraints_type constraints;
      };
      
      instantiate_type instantiate(const scheme& p) const;
      
      void unify(const type& lhs, const type& rhs);    
    
      state scope() const;

      state& def(symbol id, const scheme& p);

      const scheme& find(symbol id) const;

      variable fresh(kinds::kind k = kinds::terms) const;

    };

    

    template<class Type, class Node>
    struct inferred {
      Type type;
      Node node;
    };


    // TODO change return type?
    inferred<scheme, ast::toplevel> infer_toplevel(state& self, const ast::toplevel& node);    
    

    // some traits for builtin types
    template<class T>
    struct traits {
      static struct type type();
    };


    extern bool debug_unification;
    
  }
  
}




#endif
