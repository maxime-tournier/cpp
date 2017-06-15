#include "types.hpp"

#include <algorithm>

#include "syntax.hpp"
#include "../union_find.hpp"


#include <iostream>
#include <sstream>

namespace lisp {

  namespace types {

    template<class T>
    struct traits;

    const constant unit_type("unit"),
      boolean_type("boolean"),
      integer_type("integer"),
      real_type("real"),
      string_type("string"),
      symbol_type("symbol");

    
    template<>
    struct traits< lisp::unit > {
      static constant type() { return unit_type; }
    };

    
    template<>
    struct traits< lisp::boolean > {
      static constant type() { return boolean_type; }      
    };


    template<>
    struct traits< lisp::integer > {
      static constant type() { return integer_type; }            
    };

    template<>
    struct traits< lisp::real > {
      static constant type() { return real_type; }
    };


    template<>
    struct traits< ref<lisp::string> > {
      static constant type() { return string_type; }                  
    };

    template<>
    struct traits< lisp::symbol > {
      static constant type() { return symbol_type; }                        
    };
    

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

      void operator()(const application& self, std::ostream& out, ostream_map& osm) const {
        if(self.ctor == func_ctor) {
          bool first = true;
          for(const mono& t : self.args) {
            if(first) first = false;
            else out << " -> ";
            t.apply(ostream_visitor(), out, osm);
          }
          
        } else {
          out << self.ctor.name();
          
          if(self.args.empty()) return;

          for(const mono& t : self.args) {
            out << ' ';
            t.apply(ostream_visitor(), out, osm);
          }
        }
        
      }


      void operator()(const ref<variable>& self, std::ostream& out, ostream_map& osm) const {
        auto err = osm.emplace( std::make_pair(self, var_repr{osm.size(), false} ));
        
        out << err.first->second;
      }

    };


    static std::ostream& ostream(std::ostream& out, const scheme& self, ostream_map& osm) {
      for(const ref<variable>& var : self.vars) {
        osm.emplace( std::make_pair(var, var_repr{osm.size(), true} ) );
      }
      
      self.body.apply( ostream_visitor(), out, osm );
      return out;
    }

    std::ostream& operator<<(std::ostream& out, const scheme& self) {
      ostream_map osm;
      return ostream(out, self, osm);
    }



    using uf_type = union_find<mono>;
    static union_find<mono> uf;
    
    struct nice {
      
      mono operator()(const constant& self, uf_type& uf) const {
        return self;
      }

      mono operator()(const ref<variable>& self, uf_type& uf) const {
        mono res = uf.find(self);
        if(res != mono(self)) {
          return res.map<mono>(nice(), uf);
        }
        
        return res;
      }


      mono operator()(const application& self, uf_type& uf) const {
        mono res = uf.find(self);

        if(res != mono(self)) {
          return res.map<mono>(nice(), uf);
        }

        return map(self, [&](const mono& t) {
            return t.map<mono>(nice(), uf);
          });
        
      }
      
    };


    

    struct vars_visitor {

      void operator()(const constant&, std::set< ref<variable> >& res) const {

      }
      
      void operator()(const ref<variable>& self, std::set< ref<variable> >& res) const {
        res.insert(self);
      }


      void operator()(const application& self, std::set< ref<variable> >& res) const {
        for(const mono& t: self.args) {
          t.apply(vars_visitor(), res);
        }
      }

    };
    

    vars_type mono::vars() const {
      vars_type res;
      apply(vars_visitor(), res);
      return res;
    }
    
    scheme mono::generalize(std::size_t depth) const {

      scheme res(map<mono>(nice(), uf));
      
      const vars_type all = res.body.vars();

      // copy free variables (i.e. instantiated at a larger depth)
      auto out = std::inserter(res.vars, res.vars.begin());
      std::copy_if(all.begin(), all.end(), out, [depth](const ref<variable>& v) {
          return v->depth >= depth;
        });


      return res;
    }



    
    constructor::table_type constructor::table;
    
    using special_type = mono (*)(const ref<context>& ctx, const sexpr::list& terms);

    static mono check_lambda(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_cond(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_var(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_seq(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_def(const ref<context>& ctx, const sexpr::list& terms);            
    
    static std::map<symbol, special_type> special = {
      {kw::lambda, check_lambda},
      {kw::cond, check_cond},
      {kw::seq, check_seq},
      {kw::def, check_def},
      {symbol("var"), check_var},
    };
    
    const constructor func_ctor("->", 2);
    const constructor io_ctor("io", 1);
    const constructor ref_ctor("ref", 1);
    const constructor list_ctor("list", 1);            
    
    static mono check_expr(const ref<context>& ctx, const sexpr& e);
    static void unify(const mono& lhs, const mono& rhs);

    
    struct unification_error {
      unification_error(mono lhs, mono rhs) : lhs(lhs), rhs(rhs) { }
      mono lhs, rhs;
    };
    

    struct occurs_error {
      ref<variable> var;
      application app;
    };
    
    struct occurs_check {

      bool operator()(const constant& self, const ref<variable>& var) const {
        return false;
      }

      bool operator()(const ref<variable>& self, const ref<variable>& var) const {
        return self == var;
      }


      bool operator()(const application& self, const ref<variable>& var) const {
        for(const mono& t : self.args) {
          if( t.map<bool>(occurs_check(), var) ) return true;
        }
        
        return false;
      }
      
    };
    
    
    
    struct unify_visitor {

      bool call_reverse;
      
      unify_visitor(bool call_reverse = true) 
        : call_reverse(call_reverse) { }
      
      template<class T>
      void operator()(const T& self, const mono& rhs, uf_type& uf) const {
        
        // double dispatch
        if( call_reverse ) {
          return rhs.apply( unify_visitor(false), self, uf);
        } else {
          throw unification_error(self, rhs);
        }
      }
      
      void operator()(const ref<variable>& self, const mono& rhs, uf_type& uf) const {
        assert( uf.find(self) == self );
        assert( uf.find(rhs) == rhs );        
        
        if(rhs.is<application>() && rhs.map<bool>(occurs_check(), self)) {
          throw occurs_error{self, rhs.get<application>()};
        }
        
        uf.link(self, rhs);
      }

      
      void operator()(const constant& lhs, const constant& rhs, uf_type& uf) const {
        if( lhs != rhs ) {
          throw unification_error(lhs, rhs);
        }
      }
      

      void operator()(const application& lhs, const application& rhs, uf_type& uf) const {
        if( lhs.ctor != rhs.ctor ) {
          throw unification_error(lhs, rhs);
        }

        for(std::size_t i = 0, n = lhs.ctor.argc(); i < n; ++i) {
          unify(lhs.args[i], rhs.args[i]);
        }
        
      }

      
    };


    static mono check_lambda(const ref<context>& ctx, const sexpr::list& terms) {

      // sub-context
      ref<context> sub = make_ref<context>(ctx);

      ref<variable> result_type = variable::fresh(ctx->depth);

      struct none {};
      using maybe = variant<none, application>;
      maybe init = none();

      const sexpr::list& vars = terms->head.get<sexpr::list>();

      // build application type
      const application app_type =
        foldr(init, vars, [&ctx, &sub, &result_type](const sexpr& lhs, const maybe& rhs) {
          
            // create variable types and fill sub-context
            ref<variable> var_type = variable::fresh(ctx->depth);
            sub->def(lhs.get<symbol>(), var_type);
          
            if(rhs.is<none>()) {
              return var_type >>= result_type;
            } 
          
            return var_type >>= rhs.get<application>();
          }).get<application>();

      
      // infer body type in sub-context
      const sexpr& body = terms->tail->head;

      // unify with result type
      uf.link(result_type, check_expr(sub, body));
      
      return app_type;
    }
    



    static void unify(const mono& lhs, const mono& rhs) {
      mono l = uf.find(lhs);
      mono r = uf.find(rhs);
        
      l.apply( unify_visitor(), r, uf );
    }



    static mono check_cond(const ref<context>& ctx, const sexpr::list& items) {

      ref<variable> result_type = variable::fresh(ctx->depth);
      
      for(const sexpr& i : items ) {
        const sexpr::list& pair = i.get<sexpr::list>();

        const sexpr& test = pair->head;
        const sexpr& value = pair->tail->head;

        unify(boolean_type, check_expr(ctx, test));
        unify(result_type, check_expr(ctx, value));        
      }
      
      return result_type;
    }

    

    static mono check_app(const ref<context>& ctx, const sexpr::list& self) {
      assert(self);

      const mono func_type = check_expr(ctx, self->head);

      const list<mono> args_type = map(self->tail, [&ctx](const sexpr& e) -> mono{
          return check_expr(ctx, e);
        });

      const ref<variable> result_type = variable::fresh(ctx->depth);
      
      struct none{};
      using maybe = variant<none, application>;
      maybe init = none();
      
      const mono app_type = foldr(init, args_type, [&result_type](const mono& lhs, const maybe& rhs) {
          if(rhs.is<none>()) {
            return lhs >>= result_type;
          } 

          return lhs >>= rhs.get<application>();
        }).get<application>();

      unify(func_type, app_type);
      
      return result_type;
    }

    

    struct instantiate_visitor {
      using map_type = std::map< ref<variable>, ref<variable> >;
      
      mono operator()(const constant& self, const map_type& m) const {
        return self;
      }

      mono operator()(const ref<variable>& self, const map_type& m) const {
        auto it = m.find(self);
        if(it == m.end()) return self;
        return it->second;
      }

      mono operator()(const application& self, const map_type& m) const {
        return map(self, [&m](const mono& t) -> mono {
            return t.map<mono>(instantiate_visitor(), m);
          });
      }
      
    };
    

    mono scheme::instantiate(std::size_t depth) const {
      std::map< ref<variable>, ref<variable> > map;

      // associate each bound variable to a fresh one
      auto out = std::inserter(map, map.begin());
      std::transform(vars.begin(), vars.end(), out, [&](const ref<variable>& v) {
          return std::make_pair(v, variable::fresh(depth));
        });

      // replace each bound variable with its fresh one
      return body.map<mono>( instantiate_visitor(), map ); 
    }




    struct unbound_variable : type_error {
      unbound_variable(symbol id) : type_error("unbound variable " + id.name()) { }
    };
    
    struct expr_visitor {
      
      // literals
      template<class T>
      mono operator()(const T& self, const ref<context>& ctx) const {
        return traits<T>::type();
      }

      // variables
      mono operator()(const symbol& self, const ref<context>& ctx) const {
        // TODO at which depth should we instantiate?
        if(scheme* p = ctx->find(self)) {
          return p->instantiate(ctx->depth);
        } else {
          throw unbound_variable(self);
        }
      }

      

      mono operator()(const sexpr::list& self, const ref<context>& ctx) const {
        if(self && self->head.is<symbol>()) {
          const symbol s = self->head.get<symbol>();

          const auto it = special.find(s);
          if( it != special.end() ) {
            return it->second(ctx, self->tail);
          }

        }

        return check_app(ctx, self);
      }
      
    };


    static mono check_expr(const ref<context>& ctx, const sexpr& e) {
      try {
        
        return e.map<mono>(expr_visitor(), ctx);
        
      } catch( unification_error& e) {
        std::stringstream ss;
        scheme lhs = e.lhs.generalize(), rhs = e.rhs.generalize();

        ostream_map osm;

        ss << "cannot unify ";
        ostream(ss, lhs, osm) << " with ";
        ostream(ss, rhs, osm);

        throw type_error(ss.str());
        
      } catch( occurs_error& e ) {
        std::stringstream ss;
        scheme var = mono(e.var).generalize(), app = mono(e.app).generalize();

        ostream_map osm;
        
        ostream(ss, var, osm) << " occurs in ";
        ostream(ss, app, osm);

        throw type_error(ss.str());
      }
      
      
    }


    static mono check_def(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol& name = terms->head.get<symbol>();
      const sexpr& value = terms->tail->head;
      
      ref<variable> value_type = variable::fresh(ctx->depth);

      ref<context> sub = make_ref<context>(ctx);

      // note: value_type is bound in sub-context (i.e. monomorphic)
      sub->def(name, value_type);
      assert(sub->find(name)->vars.empty());

      unify(value_type, check_expr(sub, value));

      // generalize in current context
      ctx->def(name, value_type);

      return io_ctor(unit_type);
    }


    

    static mono check_var(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol& name = terms->head.get<symbol>();
      const sexpr& value = terms->tail->head;

      // bind values in the io monad
      mono value_type = variable::fresh(ctx->depth);
      
      unify(io_ctor(value_type), check_expr(ctx, value));

      // value_type should remain monomorphic
      ++ctx->depth;
      ctx->def(name, value_type);
      assert(ctx->find(name)->vars.empty());
      
      return io_ctor(unit_type);
    }


    static mono check_seq(const ref<context>& ctx, const sexpr::list& items) {
      mono res = io_ctor( unit_type );
      
      for(const sexpr& e : items) {

        res = io_ctor( variable::fresh(ctx->depth) );
        unify(res, check_expr(ctx, e));
        
      }
      
      return res;
    }
    


    
    scheme check(const ref<context>& ctx, const sexpr& e) {
      mono res = check_expr(ctx, e);
      return res.generalize(ctx->depth);
    }
    

    
    
    application operator>>=(const mono& lhs, const mono& rhs) {
      return application(func_ctor, {lhs, rhs});
    }
    
  }

}
