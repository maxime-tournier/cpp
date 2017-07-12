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

  
  namespace kinds {
    
    // kinds
    struct kind;

    // the monotypes kind
    struct types {
      bool operator==(const types& other) const {
        return true;
      }

      bool operator<(const types& other) const {
        return false;
      }
      
    };


    struct function;
    
    struct kind : variant<types, ref<function> > {
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
    

    // type constructors
    struct constructor;

    struct constant {
      symbol name;
      struct kind kind;

      constant(symbol name, struct kind kind)
        : name(name),
          kind(kind) { }

      bool operator==(const constant& other) const {
        return name == other.name && kind == other.kind;
      }

      bool operator<(const constant& other) const {
        return name < other.name || (name == other.name && kind < other.kind);
      }

      
    };


    extern const constructor func_ctor;
    
    
    struct variable {
      struct kind kind;
      std::size_t depth;

      variable(struct kind kind, std::size_t depth)
        : kind(kind), depth(depth) {

      }
      
    };



    

    
    struct application;

    struct constructor : variant< constant, ref<variable>, ref<application> > {

      using constructor::variant::variant;
    
      struct kind kind() const;

      // apply a type constructor of the function kind
      constructor operator()(const constructor& arg) const;
    };

    // TODO turn this into a functor goddamn it
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

      monotype(constructor self) : constructor(self) {
        if(!kind().template is<types>()) {
          std::clog << kind() << std::endl;
          assert( false );
          throw kind::error("monotypes kind expected");
        }
      }

      // not quite happy with these
      monotype(constant self) : monotype( constructor(self) ) { }
      monotype(ref<variable> self) : monotype( constructor(self) ) { }
      monotype(ref<application> self) : monotype( constructor(self) ) { }            
      
    };

    monotype operator>>=(const monotype& lhs, const monotype& rhs);
    
    

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
      typechecker(ref<env_type> env = make_ref<env_type>(),
                  ref<uf_type> uf = make_ref<uf_type>());
    
      polytype generalize(const monotype& t) const;
      monotype instantiate(const polytype& p) const;

      void unify(const monotype& lhs, const monotype& rhs);    
    
      typechecker scope() const;

      typechecker& def(const symbol& id, const polytype& p);

      const polytype& find(const symbol& id) const;

      ref<variable> fresh(kind k) const;
    };


    polytype infer(typechecker& self, const ast::toplevel& node);    

  }
  
}




#endif
