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

    // kinds
    struct kind;

    // the monotypes kind
    struct terms {
      bool operator==(const terms& other) const {
        return true;
      }

      bool operator<(const terms& other) const {
        return false;
      }
      
    };

    struct function;
    
    struct kind : variant<terms, ref<function> > {
      using kind::variant::variant;
    
      struct error : slip::error {
        using slip::error::error;
      };
    
    };

    std::ostream& operator<<(std::ostream& out, const kind& self);
    

    // function kind
    struct function {
      kind from, to;

      function(kind from, kind to) : from(from), to(to) { }
      
      bool operator==(const function& other) const {
        return from == other.from && to == other.to;
      }

    };

    ref<function> operator>>=(const kind& lhs, const kind& rhs);
    

    // type types
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


    extern const type func_ctor, io_ctor, list_ctor, ref_ctor;
    
    
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

      // apply a type constructor of the function kind
      type operator()(const type& arg) const;
    };

    // TODO turn this into a functor goddamn it
    struct application {
      type func;
      type arg;
    
      application(type func, type arg)
        : func(func),
          arg(arg) {

        if(!func.kind().is< ref<function>>()) {
          throw error("applied type constructor must have function kind");
        }
      
        if(func.kind().get< ref<function> >()->from != arg.kind() ) {
          throw error("kind error");
        }
      }
    };

    template<class F>
    static ref<application> map(const ref<application>& self, const F& f) {
      return make_ref<application>(f(self->func), f(self->arg));
    }
    
    
    type operator>>=(const type& lhs, const type& rhs);
    
    struct scheme {
      using forall_type = std::vector< ref<variable> >;
      forall_type forall;
      const type body;

      scheme(const type& body) : body(body) {
        if(body.kind() != terms()) {
          throw kind_error("monotype expected");
        }
      }
    };


    std::ostream& operator<<(std::ostream& out, const scheme& self);
    
  
    struct environment : context< environment, scheme > {
      std::size_t depth;

      environment( const ref<environment>& parent = {} )
        : environment::context(parent),
        depth( parent ? parent->depth + 1 : 0 ) {

      }

    };



  
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

    template<class T>
    struct traits {
      static constant type();
    };
    
  }
  
}




#endif
