#include "kinds.hpp"

#include "sexpr.hpp"
#include "ast.hpp"


#include <algorithm>

namespace slip {

  template<class T>
  struct traits {
    static constant type();
  };


  const constant unit_type("unit", types()),
    boolean_type("boolean", types()),
    integer_type("integer", types()),
    real_type("real", types()),
    string_type("string", types()),
    symbol_type("symbol", types());
  
  
  template<> constant traits< slip::unit >::type() { return unit_type; }
  template<> constant traits< slip::boolean >::type() { return boolean_type; }      
  template<> constant traits< slip::integer >::type() { return integer_type; }
  template<> constant traits< slip::real >::type() { return real_type; }
  template<> constant traits< slip::string >::type() { return string_type; }
  template<> constant traits< slip::symbol >::type() { return symbol_type; }

  
  
  static monotype infer(typechecker* self, const ast::expr& node);


  struct expr_visitor {

    template<class T>
    monotype operator()(const ast::literal<T>& self, typechecker* tc) const {
      return traits<T>::type();
    }


    template<class T>
    monotype operator()(const T& self, typechecker* tc) const {
      throw error("unimplemented");
    }
    
  };


  static monotype infer(typechecker* self, const ast::expr& node) {
    return node.map<monotype>(expr_visitor(), self);
  }



  struct nice {

    template<class UF>
    monotype operator()(const constant& self, UF& uf) const {
      return self;
    }


    template<class T, class UF>
    monotype operator()(const T& self, UF& uf) const {
      throw error("unimplemented");
    }
    
    // mono operator()(const ref<variable>& self, uf_type& uf) const {
    //   mono res = uf.find(self);
    //   if(res != mono(self)) {
    //     return res.map<mono>(nice(), uf);
    //   }
        
    //   return res;
    // }


    // mono operator()(const application& self, uf_type& uf) const {
    //   mono res = uf.find(self);

    //   if(res != mono(self)) {
    //     return res.map<mono>(nice(), uf);
    //   }

    //   return map(self, [&](const mono& t) {
    //       return t.map<mono>(nice(), uf);
    //     });
        
    // }
      
  };



  struct vars_visitor {

    using result_type = std::set< ref<variable> >;
    
    void operator()(const constant&, result_type& res) const { }
      
    void operator()(const ref<variable>& self, result_type& res) const {
      res.insert(self);
    }

    void operator()(const ref<application>& self, result_type& res) const {
      self->func.apply( vars_visitor(), res );
      self->arg.apply( vars_visitor(), res );      
    }
    
  };

  static vars_visitor::result_type vars(const constructor& self) {
    vars_visitor::result_type res;
    self.apply(vars_visitor(), res);
    return res;
  }


  polytype typechecker::generalize(const monotype& mono) const {
    polytype res(mono.map<monotype>(nice(), uf));

    const auto all = vars(res.body);
    
    // copy free variables (i.e. instantiated at a larger depth)
    auto out = std::back_inserter(res.forall);

    const std::size_t depth = this->env->depth;
    std::copy_if(all.begin(), all.end(), out, [depth](const ref<variable>& v) {
        return v->depth >= depth;
      });

    return res;
  }
  
  
  polytype typechecker::infer(const ast::toplevel& node) {

    const monotype res = slip::infer(this, node.get<ast::expr>());
    return generalize(res);
  }
    
  
}
