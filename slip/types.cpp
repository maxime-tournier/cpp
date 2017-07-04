#include "types.hpp"

#include <algorithm>

#include "syntax.hpp"
#include "../union_find.hpp"


#include <iostream>
#include <sstream>

namespace slip {

  namespace types {

    struct typed_sexpr {
      mono type;
      sexpr expr;

    };
    
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


    static typed_sexpr check_app(const ref<context>& ctx, const sexpr::list& self);
    
    using special_type = typed_sexpr (*)(const ref<context>& ctx, const sexpr::list& terms);

    static typed_sexpr check_lambda(const ref<context>& ctx, const sexpr::list& terms);
    static typed_sexpr check_cond(const ref<context>& ctx, const sexpr::list& terms);
    static typed_sexpr check_def(const ref<context>& ctx, const sexpr::list& terms);
    
    static typed_sexpr check_seq(const ref<context>& ctx, const sexpr::list& terms);    
    static typed_sexpr check_var(const ref<context>& ctx, const sexpr::list& terms);
    static typed_sexpr check_run(const ref<context>& ctx, const sexpr::list& terms);    

    
    static std::map<symbol, special_type> special = {
      {kw::lambda, check_lambda},
      {kw::cond, check_cond},
      {kw::def, check_def},
      {kw::var, check_var},
      
      {kw::seq, check_seq},
      {kw::run, check_run},
    };
    

    
    static typed_sexpr check_expr(const ref<context>& ctx, const sexpr& e);
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


    static typed_sexpr check_lambda(const ref<context>& ctx, const sexpr::list& terms) {

      // sub-context
      const ref<context> sub = make_ref<context>(ctx);

      const mono result_type = variable::fresh(ctx->depth);

      const sexpr::list& vars = terms->head.get<sexpr::list>();
      
      // build application type
      mono app_type = foldr(result_type, vars, [&ctx, &sub](const sexpr& lhs, const mono& rhs) {
          
          // create variable types and fill sub-context while we're at it
          ref<variable> var_type = variable::fresh(ctx->depth);
          sub->def(lhs.get<symbol>(), var_type);
          
          return var_type >>= rhs;
        });


      // remember true argcount
      app_type.get<application>().info = size(vars);
      
      // infer body type in sub-context
      const typed_sexpr body = check_expr(sub, terms->tail->head);
      
      // unify body type with result type
      uf.link(result_type, body.type);
      
      return {app_type, kw::lambda >>= vars >>= body.expr >>= sexpr::list() };
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



    static typed_sexpr check_cond(const ref<context>& ctx, const sexpr::list& items) {
      
      const ref<variable> result_type = variable::fresh(ctx->depth);

      const sexpr::list result_list = map(items, [&ctx, &result_type](const sexpr& it) -> sexpr {
      
          const sexpr::list& pair = it.get<sexpr::list>();
          
          const typed_sexpr test = check_expr(ctx, pair->head);
          const typed_sexpr value = check_expr(ctx, pair->tail->head);          
          
          unify(boolean_type, test.type);
          unify(result_type, value.type);

          return test.expr >>= value.expr >>= sexpr::list();
        });
      
      return {result_type, kw::cond >>= result_list };
    }
    

    static symbol gensym() {
      static std::size_t index = 0;

      return std::string("__arg") + std::to_string(index++);
    }


    static typed_sexpr check_app(const ref<context>& ctx, const sexpr::list& self) {
      const typed_sexpr func = check_expr(ctx, self->head);
      
      const mono result_type = variable::fresh(ctx->depth);

      const list<typed_sexpr> args = map(self->tail, [&ctx](const sexpr& e) {
          return check_expr(ctx, e);
        });

      const mono app_type = foldr(result_type, args, [&ctx](const typed_sexpr& e, mono rhs) {
          return e.type >>= rhs;
        });
      
      unify(func.type, app_type);

      const std::size_t argc = size(args);
      const std::size_t info = uf.find(func.type).get<application>().info;

      // call expression
      const sexpr::list call_expr = func.expr >>= map(args, [](const typed_sexpr& e) {
          return e.expr;
        });

      if(info == argc) {
        // regular call        
        return {result_type, call_expr};
      }
      
      if(info > argc) {
        // non-saturated call: wrap in a closure
        
        // generate additional vars
        sexpr::list vars;
        for(std::size_t i = 0; i < (info - argc); ++i) {
          vars = gensym() >>= vars;
        }

        const sexpr closure_expr = kw::lambda >>= sexpr(vars)
          >>= sexpr(concat(call_expr, vars)) >>= sexpr::list();

        // TODO FIXME
        const_cast<mono&>(uf.find(result_type)).get<application>().info = info - argc;
        
        return {result_type, closure_expr};
      }
      
      if( info < argc ) {

        const std::size_t extra = argc - info;
        std::size_t i = 0;
        
        const sexpr::list res = foldr( sexpr::list(), call_expr, [&i, extra](const sexpr& e, const sexpr::list& x) {
            const std::size_t j = i++;
            
            if(j < extra) {
              return e >>= x;
            } else if (j == extra) {
              return (e >>= sexpr::list()) >>= x;
            } else {
              return (e >>= x->head.get<sexpr::list>() ) >>= x->tail;
            }
          });
        
        // over-saturated call: split call ((func args...) args...)
        return {result_type, res};
      }

      throw error("impossibru!");
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
      typed_sexpr operator()(const T& self, const ref<context>& ctx) const {
        return {traits<T>::type(), self};
      }

      // variables
      typed_sexpr operator()(const symbol& self, const ref<context>& ctx) const {
        
        // TODO at which depth should we instantiate?
        if(scheme* p = ctx->find(self)) {
          return {p->instantiate(ctx->depth), self};
        } else {
          throw unbound_variable(self);
        }
      }

      

      typed_sexpr operator()(const sexpr::list& self, const ref<context>& ctx) const {
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


    static typed_sexpr check_expr(const ref<context>& ctx, const sexpr& e) {

      try {
        return e.map<typed_sexpr>(expr_visitor(), ctx);
        
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



    
    static typed_sexpr check_def(const ref<context>& ctx, const sexpr::list& terms) {
      
      const symbol name = terms->head.get<symbol>();
      
      const mono value_type = variable::fresh(ctx->depth);
      const mono thread = variable::fresh(ctx->depth);
      
      const ref<context> sub = make_ref<context>(ctx);

      // note: value_type is bound in sub-context (i.e. monomorphic)
      sub->def(name, value_type);
      assert(sub->find(name)->vars.empty());

      const typed_sexpr value = check_expr(sub, get(terms, 1));
      unify(value_type, value.type);
      
      // generalize in current context
      ctx->def(name, value_type);


      
      return {io_ctor(unit_type, thread), kw::def >>= name >>= value.expr >>= sexpr::list()};
    }


    
    // monadic binding
    static typed_sexpr check_var(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol& name = terms->head.get<symbol>();

      if(ctx->find_local(name)) {
        throw type_error("variable redefinition: " + name.name());
      }


      // bind values in the io monad
      const mono value_type = variable::fresh(ctx->depth);
      const mono thread = variable::fresh(ctx->depth);


      const typed_sexpr value = check_expr(ctx, get(terms, 1) );
      unify(io_ctor(value_type, thread), value.type);
      
      // value_type should remain monomorphic
      ++ctx->depth;
      ctx->def(name, value_type);
      
      assert(ctx->find(name)->vars.empty());

      // if no further computation happens, this is unit
      return {io_ctor(unit_type, thread), kw::def >>= name >>= value.expr >>= sexpr::list()};
    }
    

    // monadic sequencing
    static typed_sexpr check_seq(const ref<context>& ctx, const sexpr::list& items) {

      const mono thread = variable::fresh(ctx->depth);
      mono result_type = io_ctor( unit_type, thread );

      const sexpr::list& result_expr = map(items, [&ctx, &result_type, &thread](const sexpr& e) {
          result_type = io_ctor( variable::fresh(ctx->depth), thread );

          const typed_sexpr item = check_expr(ctx, e);
          unify(result_type, item.type);
          return item.expr;
        });
      
      return {result_type, kw::seq >>= result_expr};
    }
    

    // runST-like monad escape
    static typed_sexpr check_run(const ref<context>& ctx, const sexpr::list& items) {

      const mono thread = variable::fresh(ctx->depth);
      const mono value_type = variable::fresh(ctx->depth);      

      const ref<context> sub = make_ref<context>(ctx);

      const typed_sexpr result = check_seq(sub, items);
      
      unify( io_ctor(value_type, thread), result.type);
      
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

      return {value_type, result.expr};
    }


    static typed_sexpr check_datatype(const ref<context>& ctx, const sexpr::list& terms) {
      const symbol name = terms->head.get<sexpr::list>()->head.get<symbol>();

      const auto& v = terms->head.get<sexpr::list>()->tail;

      const list< ref<variable> > vars = map(v, [&ctx](const sexpr& e) {
          return variable::fresh(ctx->depth);
        });

      std::vector<mono> args;
      for(const auto& a : vars) {
        args.emplace_back(a);
      }
      
      constructor ctor(name, args.size());
      const mono app = application(ctor, args);

      const mono ctor_type =
        foldr(app, vars, [](const ref<variable>& lhs, const mono& rhs) {
            return lhs >>= rhs;
          });

      ctx->def(name, ctor_type);
      
      auto nil = sexpr::list();
      
      const sexpr expr = kw::def >>= name >>=
        (kw::lambda >>= v >>= (symbol("array") >>= v) >>= nil)
        >>= nil;

      std::clog << expr << std::endl;
      
      // TODO define data constructors
      return {app, expr};
    }
    

    // toplevel check
    static typed_sexpr check_toplevel_expr(const ref<context>& ctx, const sexpr& e) {
      static const mono thread = variable::fresh(ctx->depth);
      static std::size_t init = ctx->depth++; (void) init;
      
      // TODO FIXME global uf
      const typed_sexpr res = check_expr(ctx, e);

      if(res.type.is<application>() && res.type.get<application>().ctor == io_ctor) {
        // toplevel io must happen in toplevel thread
        const mono fresh = variable::fresh(ctx->depth);
        unify( io_ctor(fresh, thread), res.type);
      }

      
      
      return res; 
    }


    // toplevel
    static typed_sexpr check_toplevel(const ref<context>& ctx, const sexpr& e) {

      if(auto lst = e.get_if<sexpr::list>()) {
        if(*lst) {
          if( auto s = (*lst)->head.get_if<symbol>() ) {
            if(*s == kw::type) {
              return check_datatype(ctx, (*lst)->tail);
            }
          }
        }
      }
      
      return check_toplevel_expr(ctx, e);
    }
    

    check_type check(const ref<context>& ctx, const sexpr& e) {
      typed_sexpr res = check_toplevel(ctx, e);
      return {res.type.generalize(ctx->depth), res.expr};
    }

    
    application operator>>=(const mono& lhs, const mono& rhs) {
      const std::size_t info = rhs.is<application>() ?
        1 + rhs.get<application>().info : 1;
      
      return application(func_ctor, {lhs, rhs}, info);
    }
    
  }

}