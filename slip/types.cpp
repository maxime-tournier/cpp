#include "types.hpp"

#include <algorithm>

#include "syntax.hpp"
#include "../union_find.hpp"


#include <iostream>
#include <sstream>

namespace slip {

  namespace types {

    template<class T>
    struct traits;

    const constant unit_type("unit"),
      boolean_type("boolean"),
      integer_type("integer"),
      real_type("real"),
      string_type("string"),
      symbol_type("symbol");


    constructor::table_type constructor::table;
    
    const constructor func_ctor("->", 2);
    const constructor list_ctor("list", 1);            

    const constructor io_ctor("io", 2, 1);
    const constructor ref_ctor("ref", 2, 1);

    
    
    template<> constant traits< slip::unit >::type() { return unit_type; }
    template<> constant traits< slip::boolean >::type() { return boolean_type; }      
    template<> constant traits< slip::integer >::type() { return integer_type; }
    template<> constant traits< slip::real >::type() { return real_type; }
    template<> constant traits< ref<slip::string> >::type() { return string_type; }
    template<> constant traits< slip::symbol >::type() { return symbol_type; }
    
    
    

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
          out << self.ctor->name;

          // TODO should we print phantom types?
          const std::size_t stop = self.ctor->argc - self.ctor->phantom;
          
          for(std::size_t i = 0; i < stop; ++i) {
            out << ' ';
            self.args[i].apply(ostream_visitor(), out, osm);
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



    // TODO fixme global uf
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



    
    using special_type = mono (*)(const ref<context>& ctx, const sexpr::list& terms);

    static mono check_lambda(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_cond(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_def(const ref<context>& ctx, const sexpr::list& terms);
    
    static mono check_seq(const ref<context>& ctx, const sexpr::list& terms);    
    static mono check_var(const ref<context>& ctx, const sexpr::list& terms);
    static mono check_run(const ref<context>& ctx, const sexpr::list& terms);    

    
    static std::map<symbol, special_type> special = {
      {kw::lambda, check_lambda},
      {kw::cond, check_cond},
      {kw::def, check_def},

      {kw::seq, check_seq},
      {kw::var, check_var},
      {kw::run, check_run},
    };
    

    
    static mono check_expr(const ref<context>& ctx, const sexpr& e);
    static void unify(const mono& lhs, const mono& rhs);

    
    struct unification_error {
      unification_error(mono lhs, mono rhs) : lhs(lhs), rhs(rhs) { }
      const mono lhs, rhs;
    };
    

    struct occurs_error {
      ref<variable> var;
      mono type;
    };
    
    struct occurs_check {

      bool operator()(const constant& self, const ref<variable>& var, uf_type& uf) const {
        return false;
      }

      bool operator()(const ref<variable>& self, const ref<variable>& var, uf_type& uf) const {

        if(self->depth > var->depth) {
          // we are trying to unify var with app( ... self ... ) but var has
          // lower depth: we need to "lower" self to var's depth so that self
          // generalizes just like var
          ref<variable> lower = variable::fresh(var->depth);
          uf.link(uf.find(self), lower);
        }
        
        return self == var;
      }


      bool operator()(const application& self, const ref<variable>& var, uf_type& uf) const {
        for(const mono& t : self.args) {
          if( uf.find(t).map<bool>(occurs_check(), var, uf) ) return true;
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
        
        if( mono(self) != rhs && rhs.map<bool>(occurs_check(), self, uf)) {
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

        for(std::size_t i = 0, n = lhs.ctor->argc; i < n; ++i) {
          unify(lhs.args[i], rhs.args[i]);
        }
        
      }

      
    };


    static mono check_lambda(const ref<context>& ctx, const sexpr::list& terms) {

      // sub-context
      const ref<context> sub = make_ref<context>(ctx);

      const mono result_type = variable::fresh(ctx->depth);

      const sexpr::list& vars = terms->head.get<sexpr::list>();
      
      // build application type
      const mono app_type = foldr(result_type, vars, [&ctx, &sub](const sexpr& lhs, const mono& rhs) {
          
          // create variable types and fill sub-context while we're at it
          ref<variable> var_type = variable::fresh(ctx->depth);
          sub->def(lhs.get<symbol>(), var_type);
          
          return var_type >>= rhs;
        });

      
      // infer body type in sub-context
      const sexpr& body = terms->tail->head;

      // unify with result type
      uf.link(result_type, check_expr(sub, body));
      
      return app_type;
    }
    


    struct debug {
      debug(const mono& type) : type(type) { }
      const mono& type;

      friend  std::ostream& operator<<(std::ostream& out, const debug& self) {
        static ostream_map osm;
        self.type.apply( ostream_visitor(), out, osm );
        return out;
      }
      
    };


    struct indent {
      indent(std::size_t level) : level(level) { }
      const std::size_t level;

      friend std::ostream& operator<<(std::ostream& out, const indent& self) {
        for(std::size_t i = 0; i < self.level; ++i) out << "  ";
        return out;
      }
      
    };


    
    
    static void unify(const mono& lhs, const mono& rhs) {

      // static std::size_t level = 0;
      // std::clog << indent(level++) << "unifying (" << debug(lhs) << ") with (" << debug(rhs) << ")" << std::endl;
      
      const mono l = uf.find(lhs);
      const mono r = uf.find(rhs);

      l.apply( unify_visitor(), r, uf );

      // std::clog << indent(--level) << "result: " << debug( uf.find(l) ) << std::endl;
    }



    static mono check_cond(const ref<context>& ctx, const sexpr::list& items) {

      const ref<variable> result_type = variable::fresh(ctx->depth);
      
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
      
      const mono result_type = variable::fresh(ctx->depth);

      const mono app_type = foldr(result_type, self->tail, [&ctx](const sexpr& e, mono rhs) {
          return check_expr(ctx, e) >>= rhs;
        });
      
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
        scheme var = mono(e.var).generalize(), type = e.type.generalize();

        ostream_map osm;
        
        ostream(ss, var, osm) << " occurs in ";
        ostream(ss, type, osm);

        throw type_error(ss.str());
      }
      
    }



    
    static mono check_def(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol& name = terms->head.get<symbol>();
      const sexpr& value = terms->tail->head;
      
      const mono value_type = variable::fresh(ctx->depth);
      const mono thread = variable::fresh(ctx->depth);
      
      const ref<context> sub = make_ref<context>(ctx);

      // note: value_type is bound in sub-context (i.e. monomorphic)
      sub->def(name, value_type);
      assert(sub->find(name)->vars.empty());

      unify(value_type, check_expr(sub, value));

      // generalize in current context
      ctx->def(name, value_type);

      return io_ctor(unit_type, thread);
    }


    
    // monadic binding
    static mono check_bind(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol& name = terms->head.get<symbol>();
      const sexpr& value = terms->tail->head;

      // bind values in the io monad
      const mono value_type = variable::fresh(ctx->depth);
      const mono thread = variable::fresh(ctx->depth);
      
      unify(io_ctor(value_type, thread), check_expr(ctx, value));

      // value_type should remain monomorphic
      ++ctx->depth;
      ctx->def(name, value_type);
      assert(ctx->find(name)->vars.empty());

      // if no further computation happens, this is unit
      return io_ctor(unit_type, thread);
    }


    static mono check_var(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol& name = terms->head.get<symbol>();
      if(ctx->find_local(name)) {
        throw type_error("variable redefinition: " + name.name());
      }

      return check_bind(ctx, terms);
    }

    // monadic sequencing
    static mono check_seq(const ref<context>& ctx, const sexpr::list& items) {

      const mono thread = variable::fresh(ctx->depth);
      mono res = io_ctor( unit_type, thread );
      
      for(const sexpr& e : items) {
        res = io_ctor( variable::fresh(ctx->depth), thread );
        unify(res, check_expr(ctx, e));
      }
      
      return res;
    }
    

    // runST-like monad escape
    static mono check_run(const ref<context>& ctx, const sexpr::list& items) {
      const mono thread = variable::fresh(ctx->depth);
      const mono value_type = variable::fresh(ctx->depth);      

      const ref<context> sub = make_ref<context>(ctx);

      unify( io_ctor(value_type, thread), check_seq(sub, items));

      const scheme gen_thread = thread.generalize(ctx->depth);
      const scheme gen_value_type = value_type.generalize(ctx->depth);            

      const vars_type vars = gen_value_type.body.vars();
      
      if( vars.find(gen_thread.body.cast<ref<variable>>() ) != vars.end()) {
        // thread appears in result type: this is impossible with rank-2 types
        throw type_error("computation thread escape");
      }
      
      if(gen_thread.vars.empty()) {
        // thread variable is bound: computation can access outer state and
        // maybe unpure
        throw type_error("computation references outer thread");
      }

      return value_type;
    }



    

    // toplevel check
    scheme check(const ref<context>& ctx, const sexpr& e) {
      static const mono thread = variable::fresh(ctx->depth);
      static std::size_t init = ctx->depth++; (void) init;
      
      // TODO FIXME global uf
      const mono res = check_expr(ctx, e).map<mono>(nice(), uf);
      
      if(res.is<application>() && res.get<application>().ctor == io_ctor) {

        // toplevel io must happen in toplevel thread
        const mono fresh = variable::fresh(ctx->depth);
        unify( io_ctor(fresh, thread), res);
        
      }
      
      return res.generalize(ctx->depth);
    }
    

    
    
    application operator>>=(const mono& lhs, const mono& rhs) {
      return application(func_ctor, {lhs, rhs});
    }
    
  }

}
