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

  
  // kinds
  struct kind;

  struct types {
    bool operator==(const types& other) const {
      return true;
    }
  };

  
  struct function;
    
  struct kind : variant<types, ref<function> > {
    
    struct error : slip::error {
      using slip::error::error;
    };
    
  };
  
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
    slip::kind kind;
  };
  
  struct variable {
    std::size_t depth;
    slip::kind kind;
  };


  struct application;

  struct kind_visitor {

    template<class T>
    slip::kind operator()(const T& self) const { return self.kind; }

    slip::kind operator()(const ref<application>& self) const;
    
  };
  

  struct constructor : variant< constant, variable, ref<application> > {

    using constructor::variant::variant;
    
    slip::kind kind() const {
      return map<slip::kind>(kind_visitor());
    }
    
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

  slip::kind kind_visitor::operator()(const ref<application>& self) const {
    return self->func.kind().get< ref<function> >()->to;
  }  


  struct monotype : constructor {

    template<class T>
    monotype(const T& t) : constructor(t) {
      if(!kind().template is<types>()) {
        throw kind::error("monotypes kind expected");
      }
    }
    
  };

  struct polytype {
    std::vector< variable > forall;
    monotype body;
  };

  
  struct environment : context< environment, polytype >  { };
  
  
  class state {
    using env_type = environment;
    ref<env_type> env;

    using uf_type = union_find<constructor>;
    ref< uf_type > uf;
    
  public:
    state();
    
    polytype generalize(monotype t) const;
    monotype instantiate(polytype p) const;

    void unify(monotype lhs, monotype rhs);    
    
    state scope() const;

    state& def(symbol id, polytype p);
    
  };
  

  
  
}




#endif
