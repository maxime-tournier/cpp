#ifndef SLIP_KINDS_HPP
#define SLIP_KINDS_HPP

#include "variant.hpp"
#include "../ref.hpp"
#include "../union_find.hpp"

#include "symbol.hpp"
#include "error.hpp"
#include "context.hpp"

#include <vector>

#include <iostream>

namespace slip {

  namespace ast {
    struct toplevel;
  }

  
  namespace types {

    ////////////////////
    // kinds
    struct kind;


    struct simple_kind {
      template<class Derived>
      friend bool operator==(const Derived& self, const Derived& other) { return true; }

      template<class Derived>      
      friend bool operator<(const Derived& self, const Derived& other) { return false; }      
    };

    // the kind of terms
    struct terms : simple_kind { };

    // the kind of rows
    struct rows : simple_kind { };    

    // kind of type constructors
    struct constructor {
      ref<kind> from, to;
      
      constructor(const kind& from, const kind& to)
        : from( make_ref<kind>(from)),
          to( make_ref<kind>(to)) { }
      
      bool operator==(const constructor& other) const;
      bool operator<(const constructor& other) const;      
    };

    constructor operator>>=(const kind& lhs, const kind& rhs);


    struct kind : variant<terms, rows, constructor> {
      using kind::variant::variant;
    };

    std::ostream& operator<<(std::ostream& out, const kind& self);
    

    

    ////////////////////    
    // types
    struct type;

    struct constant {
      symbol name;
      struct kind kind;
      
      constant(symbol name, struct kind kind = terms())
        : name(name),
          kind(kind) { }

      bool operator==(const constant& other) const {
        return name == other.name && kind == other.kind;
      }

      bool operator<(const constant& other) const {
        return name < other.name || (name == other.name && kind < other.kind);
      }
      
    };

    static constant make_constant(symbol name, struct kind kind = terms() ) {
      return {name, kind};
    }
    

    
    struct variable {
      std::size_t id;
      
      struct kind kind;
      std::size_t depth;        // TODO const?

      variable(struct kind kind, std::size_t depth)
        : kind(kind), depth(depth) {
        static std::size_t count = 0;
        id = count++;
      }

      bool operator==(const variable& other) const { return id == other.id; }
      bool operator<(const variable& other) const { return id < other.id; }
      
    };

    
    struct application;


    struct type : variant< constant, variable, ref<application> > {
      
      using type::variant::variant;
    
      struct kind kind() const;

      // apply a type constructor to a type.
      // TODO only for applications
      type operator()(const type& arg) const;
      
    };

    
    // application of a type constructor to a type
    struct application {
      type func;
      type arg;

      application(type func, type arg);


      // used to remember "true" argcount in lambdas when currying
      std::size_t argc = 0;
    };

    template<class F>
    static ref<application> map(const ref<application>& self, const F& f) {
      return make_ref<application>(f(self->func), f(self->arg));
    }
    

    // type schemes
    struct scheme {
      using forall_type = std::vector< variable >;
      forall_type forall;
      const type body;

      scheme(const type& body);
      
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



    // inference state monad
    class state {
      using env_type = environment;
      ref<env_type> env;

      using uf_type = union_find<type>;
      ref< uf_type > uf;

    public:
      // type constructors
      using ctor_type = std::map<symbol, type>;
      ref<ctor_type> ctor;

      
      // TODO nested type environments for type variables

    public:
      state(ref<env_type> env = make_ref<env_type>(),
            ref<uf_type> uf = make_ref<uf_type>(),
            ref<ctor_type> ctor = make_ref<ctor_type>());
      
      scheme generalize(const type& t) const;
      type instantiate(const scheme& p) const;

      void unify(const type& lhs, const type& rhs);    
    
      state scope() const;

      state& def(symbol id, const scheme& p);

      const scheme& find(symbol id) const;

      variable fresh(kind k = terms()) const;

      
    };


    template<class Type, class Node>
    struct inferred {
      Type type;
      Node node;
    };

    
    inferred<scheme, ast::toplevel> infer(state& self, const ast::toplevel& node);    
    

    // some traits
    template<class T>
    struct traits {
      static struct type type();
    };
    
  }
  
}




#endif
