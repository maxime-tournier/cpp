#include "types.hpp"

#include "sexpr.hpp"
#include "ast.hpp"

#include <algorithm>
#include <sstream>

namespace slip {

  namespace types {


    template<class ... C>
    static std::ostream& debug(std::ostream& out, C&& ... c);
    
    ref<function> operator>>=(const kind& lhs, const kind& rhs) {
      return make_ref<function>(lhs, rhs);
    }

    const constructor func_ctor = constant("->", monotypes() >>= monotypes() >>= monotypes() );
    const constructor io_ctor = constant("io", monotypes() >>= monotypes() );
    const constructor list_ctor = constant("list", monotypes() >>= monotypes() );        
    
    constructor constructor::operator()(const constructor& arg) const {
      return make_ref<application>(*this, arg);
    }
    
    monotype operator>>=(const monotype& lhs, const monotype& rhs) {
      return (func_ctor(lhs))(rhs);

    }
    


    
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


    struct kind_ostream {

      void operator()(monotypes, std::ostream& out) const {
        out << '*';
      }

      void operator()(const ref<function>& self, std::ostream& out) const {
        // TODO parentheses
        out << self->from << " -> " << self->to << std::endl;
      }
      
      
    };
    
    std::ostream& operator<<(std::ostream& out, const kind& self) {
      self.apply(kind_ostream(), out);
      return out;      
    }


    
    
    template<class T>
    struct traits {
      static constant type();
    };


    const constant unit_type("unit", monotypes()),
      boolean_type("boolean", monotypes()),
      integer_type("integer", monotypes()),
      real_type("real", monotypes()),
      string_type("string", monotypes()),
      symbol_type("symbol", monotypes());
  
  
    template<> constant traits< slip::unit >::type() { return unit_type; }
    template<> constant traits< slip::boolean >::type() { return boolean_type; }      
    template<> constant traits< slip::integer >::type() { return integer_type; }
    template<> constant traits< slip::real >::type() { return real_type; }
    template<> constant traits< slip::string >::type() { return string_type; }
    template<> constant traits< slip::symbol >::type() { return symbol_type; }


    typechecker::typechecker(ref<env_type> env, ref<uf_type> uf) 
      : env( env ),
        uf( uf ) {
      
    }

    
    static monotype infer(typechecker& self, const ast::expr& node);
    


    // unification
    struct unification_error {
      unification_error(constructor lhs, constructor rhs) : lhs(lhs), rhs(rhs) { }
      const constructor lhs, rhs;
    };

    
    struct occurs_error {
      ref<variable> var;
      constructor type;         // TODO do we want monotype here?
    };



    template<class UF>
    struct occurs_check {

      bool operator()(const constant& self, const ref<variable>& var, UF& uf) const {
        return false;
      }

      bool operator()(const ref<variable>& self, const ref<variable>& var, UF& uf) const {
        // TODO not sure if this should be done here
        
        if(self->depth > var->depth) {
          // we are trying to unify var with a type constructor containing self,
          // but var has smaller depth: we need to "raise" self to var's depth
          // so that self generalizes just like var
          const constructor raised = make_ref<variable>(var->kind, var->depth);

          assert(uf.find(self).kind() == raised.kind());
          uf.link(uf.find(self), raised);
        }
        
        return self == var;
      }


      bool operator()(const ref<application>& self, const ref<variable>& var, UF& uf) const {

        if( uf.find(self->arg).template map<bool>(occurs_check(), var, uf) ) {
          return true;
        }
        
        if( uf.find(self->func).template map<bool>(occurs_check(), var, uf) ) {
          return true;
        }
        
        return false;
      }
      
    };

    

    template<class UF>
    struct unify_visitor {

      const bool try_reverse;
      
      unify_visitor(bool try_reverse = true) 
        : try_reverse(try_reverse) { }
      
      template<class T>
      void operator()(const T& self, const constructor& rhs, UF& uf) const {
        
        // double dispatch
        if( try_reverse ) {
          return rhs.apply( unify_visitor(false), self, uf);
        } else {
          throw unification_error(self, rhs);
        }
      }


      void operator()(const ref<variable>& self, const constructor& rhs, UF& uf) const {
        assert( uf.find(self) == self );
        assert( uf.find(rhs) == rhs );        
        
        if( constructor(self) != rhs && rhs.map<bool>(occurs_check<UF>(), self, uf)) {
          throw occurs_error{self, rhs.get< ref<application> >()};
        }

        assert(self->kind == rhs.kind());
        
        // debug( std::clog << "linking", constructor(self), rhs ) << std::endl;
        uf.link(self, rhs);
        
      }


      void operator()(const constant& lhs, const constant& rhs, UF& uf) const {
        if( !(lhs == rhs) ) {
          throw unification_error(lhs, rhs);
        }
      }
      

      void operator()(const ref<application>& lhs, const ref<application>& rhs, UF& uf) const {
        uf.find(lhs->arg).apply( unify_visitor(), uf.find(rhs->arg), uf);        
        uf.find(lhs->func).apply( unify_visitor(), uf.find(rhs->func), uf);
      }

      
    };

    


    
    // expression inference
    struct expr_visitor {

      
      template<class T>
      monotype operator()(const ast::literal<T>& self, typechecker& tc) const {
        return traits<T>::type();
      }

      
      monotype operator()(const ast::variable& self, typechecker& tc) const {
        const polytype& p = tc.find(self.name);
        return tc.instantiate(p);
      }

      
      monotype operator()(const ref<ast::lambda>& self, typechecker& tc) const {

        typechecker sub = tc.scope();

        // create/define arg types
        const list< ref<variable> > args = map(self->args, [&](const symbol& s) {
            const ref<variable> var = tc.fresh();
            // note: var stays monomorphic after generalization
            sub.def(s, sub.generalize(var) );
            return var;
          });
        
        // infer body type in subcontext
        const monotype body_type = infer(sub, self->body);

        // return complete application type
        return foldr(body_type, args, [](const ref<variable>& lhs,
                                         const monotype& rhs) -> monotype {
            return lhs >>= rhs;
          });
        
      }



      monotype operator()(const ref<ast::application>& self, typechecker& tc) const {

        const monotype func = infer(tc, self->func);

        // infer arg types
        const list<monotype> args = map(self->args, [&](const ast::expr& e) {
            return infer(tc, e);
          });

        // construct function type
        const monotype result = tc.fresh();
        
        const monotype sig = foldr(result, args, [&](const monotype& lhs,
                                                     const monotype& rhs) {
                                     return lhs >>= rhs;
                                   });

        try{
          tc.unify(func, sig);
        } catch( occurs_error ) {
          throw type_error("occurs check");
        }

        return result;
      }


      monotype operator()(const ref<ast::condition>& self, typechecker& tc) const {
        const monotype result = tc.fresh();

        for(const ast::condition::branch& b : self->branches) {

          const monotype test = infer(tc, b.test);
          tc.unify(boolean_type, test);
          
          const monotype value = infer(tc, b.value);
          tc.unify(value, result);
        };

        return result;
      }


      // let-binding
      monotype operator()(const ref<ast::definition>& self, typechecker& tc) const {

        const monotype value = tc.fresh();
        
        typechecker sub = tc.scope();

        // note: value is bound in sub-context (monomorphic)
        sub.def(self->id, sub.generalize(value));

        tc.unify(value, infer(sub, self->value));

        tc.def(self->id, tc.generalize(value));
        
        return io_ctor( unit_type );
        
      }


      // monadic binding
      monotype operator()(const ref<ast::binding>& self, typechecker& tc) const {

        const monotype value = tc.fresh();

        // note: value is bound in sub-context (monomorphic)
        tc = tc.scope();
        
        tc.def(self->id, tc.generalize(value));

        tc.unify(io_ctor(value), infer(tc, self->value));

        tc.find(self->id);
        
        return io_ctor( unit_type );
        
      }
      

      monotype operator()(const ref<ast::sequence>& self, typechecker& tc) const {
        monotype res = io_ctor( unit_type );

        for(const ast::expr& e : self->items) {

          res = io_ctor( tc.fresh() );
          tc.unify( res, infer(tc, e));          
        }
          
        return res;
      }

      
      
      
      template<class T>
      monotype operator()(const T& self, typechecker& tc) const {
        throw error("infer unimplemented");
      }
      
    };


    static monotype infer(typechecker& self, const ast::expr& node) {
      try{
        return node.map<monotype>(expr_visitor(), self);
      } catch( unification_error& e )  {
        std::stringstream ss;

        debug(ss, "cannot unify: ", e.lhs, " with: ", e.rhs);
        
        throw type_error(ss.str());
      }
    }



    
    // finding nice representants
    struct nice {

      template<class UF>
      constructor operator()(const constant& self, UF& uf) const {
        return self;
      }


      template<class UF>
      constructor operator()(const ref<variable>& self, UF& uf) const {

        const constructor res = uf->find(self);
        
        if(res == monotype(self)) {
          // debug(std::clog << "nice: ", constructor(self), res) << std::endl;          
          return res;
        }
        
        return res.map<constructor>(nice(), uf);
      }


      template<class UF>
      constructor operator()(const ref<application>& self, UF& uf) const {
        return map(self, [&](const constructor& c) {
            return c.map<constructor>(nice(), uf);
          });
      }
      
    };




    // instantiation
    struct instantiate_visitor {
      using map_type = std::map< ref<variable>, ref<variable> >;
      
      constructor operator()(const constant& self, const map_type& m) const {
        return self;
      }

      constructor operator()(const ref<variable>& self, const map_type& m) const {
        auto it = m.find(self);
        if(it == m.end()) return self;
        return it->second;
      }

      constructor operator()(const ref<application>& self, const map_type& m) const {
        return map(self, [&](const constructor& c) {
            return c.map<constructor>(instantiate_visitor(), m);
          });
      }
      
    };
    


    monotype typechecker::instantiate(const polytype& poly) const {
      instantiate_visitor::map_type map;

      // associate each bound variable to a fresh one
      auto out = std::inserter(map, map.begin());
      std::transform(poly.forall.begin(), poly.forall.end(), out, [&](const ref<variable>& v) {
          return std::make_pair(v, fresh(v->kind));
        });
      
      return poly.body.map<monotype>(instantiate_visitor(), map);
    }




    // env lookup/def
    struct unbound_variable : type_error {
      unbound_variable(symbol id) : type_error("unbound variable " + id.name()) { }
    };

    
    const polytype& typechecker::find(symbol id) const {

      if(polytype* p = env->find(id)) {
        return *p;
      }

      throw unbound_variable(id);
    }


    typechecker& typechecker::def(symbol id, const polytype& p) {
      auto res = env->locals.emplace( std::make_pair(id, p) );
      if(!res.second) throw error("redefinition of " + id.name());
      return *this;
    }
    

    ref<variable> typechecker::fresh(kind k) const {
      return make_ref<variable>(k, env->depth);
    }


    typechecker typechecker::scope() const {
      // TODO should we use nested union-find too?
      ref<env_type> sub = make_ref<env_type>(env);

      return typechecker( sub, uf);
    }
    


    // variables
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
    
    

    // generalization
    polytype typechecker::generalize(const monotype& mono) const {
      // debug(std::clog << "gen: ", mono) << std::endl;
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

      void operator()(const constant& self, std::ostream& out, 
                      ostream_map& osm) const {
        out << self.name;
      }

      void operator()(const ref<variable>& self, std::ostream& out, 
                      ostream_map& osm) const {
        auto err = osm.emplace( std::make_pair(self, var_repr{osm.size(), false} ));
        out << err.first->second;
      }

      void operator()(const ref<application>& self, std::ostream& out,
                      ostream_map& osm) const {
        // TODO parentheses

        if(self->func == func_ctor) {
          self->arg.apply(ostream_visitor(), out, osm);
          out << ' ';
          self->func.apply(ostream_visitor(), out, osm);
        } else {
          self->func.apply(ostream_visitor(), out, osm);
          out << ' ';
          self->arg.apply(ostream_visitor(), out, osm);
        }
         
      }

      
    };


    static std::ostream& ostream(std::ostream& out, const polytype& self, 
                                 ostream_map& osm) {
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



    void typechecker::unify(const constructor& lhs, const constructor& rhs) {
      // debug( std::clog << "unifying: ", lhs, rhs) << std::endl;
      uf->find(lhs).apply( unify_visitor<uf_type>(), uf->find(rhs), *uf);
    }


    static std::ostream& debug_aux(std::ostream& out,
                                   const constructor& self,
                                   ostream_map& osm) {
      self.apply(ostream_visitor(), out, osm);
      return out;
    }


    static std::ostream& debug_aux(std::ostream& out,
                                   const std::string& self,
                                   ostream_map& osm) {
      return out << self;
    }

    
    template<class ... C>
    static std::ostream& debug(std::ostream& out, C&& ... c) {
      ostream_map osm;
      const int expand[] = {
        
        ( debug_aux(out, c, osm), 0)...
      }; (void) expand;
      
      return out;
    }
  }
  
}
