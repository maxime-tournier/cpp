#ifndef SLIP_KINDS_HPP
#define SLIP_KINDS_HPP

#include "../variant.hpp"
#include "../ref.hpp"
#include "../union_find.hpp"

#include "symbol.hpp"
#include "error.hpp"
#include "context.hpp"

#include <vector>

namespace slip {

  namespace ast {
    struct toplevel;
  }

  
  namespace kinds {
    
    // kinds
    struct kind;

    // the monotypes kind
    struct types {
      bool operator==(const types& other) const {
        return true;
      }
    };


    struct function;
    
    struct kind : variant<types, ref<function> > {
      using kind::variant::variant;
    
      struct error : slip::error {
        using slip::error::error;
      };
    
    };

    // function kind
    struct function {
      kind from, to;

      bool operator==(const function& other) const {
        return from == other.from && to == other.to;
      }
    };
  

    // type constructors
    struct constructor;
  
    struct constant {
      symbol name;
      struct kind kind;

      constant(symbol name, struct kind kind)
        : name(name),
          kind(kind) {

      }
    };
  
    struct variable {
      std::size_t depth;
      struct kind kind;
    };


    struct application;

    struct constructor : variant< constant, ref<variable>, ref<application> > {

      using constructor::variant::variant;
    
      struct kind kind() const;
    
    };


    struct application {
      constructor func;
      constructor arg;
    
      application(constructor func, constructor arg)
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

 
    struct monotype : constructor {

      template<class T>
      monotype(const T& t) : constructor(t) {
        if(!kind().template is<types>()) {
          throw kind::error("monotypes kind expected");
        }
      }
    
    };

    struct polytype {
      using forall_type = std::vector< ref<variable> >;
      forall_type forall;
      monotype body;

      polytype(const monotype& body) : body(body) { }
    };


    std::ostream& operator<<(std::ostream& out, const polytype& self);
    
  
    struct environment : context< environment, polytype > {
      std::size_t depth;

      environment( const ref<environment>& parent = {} )
        : environment::context(parent),
        depth( parent ? parent->depth + 1 : 0 ) {

      }

    };



  
    class typechecker {
      using env_type = environment;
      ref<env_type> env;

      using uf_type = union_find<constructor>;
      ref< uf_type > uf;
    
    public:
      typechecker();
    
      polytype generalize(const monotype& t) const;
      monotype instantiate(const polytype& p) const;

      void unify(const monotype& lhs, const monotype& rhs);    
    
      typechecker scope() const;

      typechecker& def(const symbol& id, const polytype& p);

    };


    polytype infer(typechecker& self, const ast::toplevel& node);    

  }
  
}




#endif
