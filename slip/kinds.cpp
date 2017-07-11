#include "kinds.hpp"

#include "sexpr.hpp"
#include "ast.hpp"


#include <algorithm>

namespace slip {

  namespace kinds {



    struct kind_visitor {
      kind operator()(const constant& self) const { return self.kind; }
      kind operator()(const ref<variable>& self) const { return self->kind; }    

      kind operator()(const ref<application>& self) const  {
        return self->func.kind().get< ref<function> >()->to;
      }  
    
    };
  

    struct kind constructor::kind() const {
      return map<struct kind>(kind_visitor());
    }
    
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


    typechecker::typechecker()
      : env( make_ref<environment>() ),
        uf( make_ref<uf_type>() ) {

    }

    
    static monotype infer(typechecker& self, const ast::expr& node);
    

    struct expr_visitor {

      template<class T>
      monotype operator()(const ast::literal<T>& self, typechecker& tc) const {
        return traits<T>::type();
      }


      monotype operator()(const ast::variable& self, typechecker& tc) const {
        const polytype& p = tc.find(self.name);
        return tc.instantiate(p);
      }

      template<class T>
      monotype operator()(const T& self, typechecker& tc) const {
        throw error("infer unimplemented");
      }
      
    };


    static monotype infer(typechecker& self, const ast::expr& node) {
      return node.map<monotype>(expr_visitor(), self);
    }



    struct nice {

      template<class UF>
      monotype operator()(const constant& self, UF& uf) const {
        return self;
      }


      template<class T, class UF>
      monotype operator()(const T& self, UF& uf) const {
        throw error("nice unimplemented");
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


    struct instantiate_visitor {
      using map_type = std::map< ref<variable>, ref<variable> >;
      
      monotype operator()(const constant& self, const map_type& m) const {
        return self;
      }

      monotype operator()(const ref<variable>& self, const map_type& m) const {
        auto it = m.find(self);
        if(it == m.end()) return self;
        return it->second;
      }

      monotype operator()(const ref<application>& self, const map_type& m) const {
        return make_ref<application>( self->func.map<monotype>(instantiate_visitor(), m),
                                      self->func.map<monotype>(instantiate_visitor(), m) );
      }
      
    };

    struct unbound_variable : type_error {
      unbound_variable(symbol id) : type_error("unbound variable " + id.name()) { }
    };

    
    const polytype& typechecker::find(const symbol& id) const {

      if(polytype* p = env->find(id)) {
        return *p;
      }

      throw unbound_variable(id);
    }
    

    ref<variable> typechecker::fresh(kind k) const {
      return make_ref<variable>(k, env->depth);
    }

    monotype typechecker::instantiate(const polytype& poly) const {
      instantiate_visitor::map_type map;

      // associate each bound variable to a fresh one
      auto out = std::inserter(map, map.begin());
      std::transform(poly.forall.begin(), poly.forall.end(), out, [&](const ref<variable>& v) {
          return std::make_pair(v, fresh(v->kind));
        });
      
      return poly.body.map<monotype>(instantiate_visitor(), map);
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
  
  
    polytype infer(typechecker& self, const ast::toplevel& node) {

      const monotype res = infer(self, node.get<ast::expr>());
      return self.generalize(res);
    }


    // ostream


    struct var_repr {
      std::size_t index;
      bool quantified;
    };

    static std::ostream& operator<<(std::ostream& out, const var_repr& self) {
      if( self.quantified ) out << "'";
      else out << "!";
      return out << char('a' + self.index);
    }

    
    using ostream_map = std::map< ref<variable>, var_repr >;
    
    struct ostream_visitor {

      void operator()(const constant& self, std::ostream& out, ostream_map& osm) const {
        out << self.name;
      }


      void operator()(const ref<variable>& self, std::ostream& out, ostream_map& osm) const {
        auto err = osm.emplace( std::make_pair(self, var_repr{osm.size(), false} ));
        out << err.first->second;
      }

      template<class T>
      void operator()(const T& self, std::ostream& out, ostream_map& osm) const {
        throw error("ostream unimplemented");
      }
    };


    static std::ostream& ostream(std::ostream& out, const polytype& self, ostream_map& osm) {
      for(const ref<variable>& var : self.forall) {
        osm.emplace( std::make_pair(var, var_repr{osm.size(), true} ) );
      }
      
      self.body.apply( ostream_visitor(), out, osm );
      return out;
    }




    std::ostream& operator<<(std::ostream& out, const polytype& self) {
      ostream_map osm;
      return ostream(out, self, osm);
    }
    
  }
  
}
