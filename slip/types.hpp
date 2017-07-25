#ifndef SLIP_KINDS_HPP
#define SLIP_KINDS_HPP

#include "../variant.hpp"
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

    // the kind of terms
    struct terms {
      bool operator==(const terms& other) const { return true; }
      bool operator<(const terms& other) const { return false; }
    };


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


    // the kind of rows
    struct rows {
      bool operator==(const rows& other) const { return true; }
      bool operator<(const rows& other) const { return false; }
    };
                
    
    
    struct kind : variant<terms, constructor, rows > {
      using kind::variant::variant;
      
      struct error : slip::error {
        using slip::error::error;
      };
    
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

      bool operator==(const constant& other) const;
      
      bool operator<(const constant& other) const;
      
    };


    
    struct variable {
      struct kind kind;
      std::size_t depth;

      variable(struct kind kind, std::size_t depth)
        : kind(kind), depth(depth) {

      }
      
    };


    
    
    struct application;

    struct type : variant< constant, ref<variable>, ref<application> > {

      using type::variant::variant;
    
      struct kind kind() const;

      // apply a type constructor of the constructor kind
      type operator()(const type& arg) const;
    };

    // TODO turn this into a functor goddamn it
    struct application {
      type func;
      type arg;
    
      application(type func, type arg)
        : func(func),
          arg(arg) {

        if(!func.kind().is< constructor>()) {
          throw error("applied type constructor must have constructor kind");
        }
      
        if(*func.kind().get< constructor >().from != arg.kind() ) {
          throw error("kind error");
        }
      }
    };

    template<class F>
    static ref<application> map(const ref<application>& self, const F& f) {
      return make_ref<application>(f(self->func), f(self->arg));
    }
    

    // type schemes
    struct scheme {
      using forall_type = std::vector< ref<variable> >;
      forall_type forall;
      const type body;

      scheme(const type& body);
      
    };

    std::ostream& operator<<(std::ostream& out, const scheme& self);
    
    

    // some type constructors
    extern const type func_ctor, io_ctor, list_ctor, ref_ctor;
    
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
      state(ref<env_type> env = make_ref<env_type>(),
            ref<uf_type> uf = make_ref<uf_type>());
    
      scheme generalize(const type& t) const;
      type instantiate(const scheme& p) const;

      void unify(const type& lhs, const type& rhs);    
    
      state scope() const;

      state& def(symbol id, const scheme& p);

      const scheme& find(symbol id) const;

      ref<variable> fresh(kind k = terms()) const;
    };

    scheme infer(state& self, const ast::toplevel& node);    


    // some traits
    template<class T>
    struct traits {
      static constant type();
    };
    
  }
  
}




#endif
