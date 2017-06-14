#include "types.hpp"

#include <algorithm>

#include "syntax.hpp"
#include "../union_find.hpp"


#include <iostream>


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
    

    struct ostream_visitor {

      void operator()(const constant& self, std::ostream& out) const {
        out << self.name;
      }

      void operator()(const application& self, std::ostream& out) const {
        if(self.ctor == func_ctor) {
          bool first = true;
          for(const mono& t : self.args) {
            if(first) first = false;
            else out << " -> ";
            out << t;
          }
          
        } else {
          out << self.ctor.name();
          
          if(self.args.empty()) return;
          out << ' ';
          
          out << make_list<mono>(self.args.begin(), self.args.end());
        }
        
      }


      void operator()(const ref<variable>& self, std::ostream& out) const {
        // TODO we need some context
        out << "#<var: " << self.get() << ">";
      }
      
    };

    
    std::ostream& operator<<(std::ostream& out, const mono& self) {
      self.apply(ostream_visitor(), out);
      return out;
    }



    std::ostream& operator<<(std::ostream& out, const scheme& self) {
      // TODO build context
      return out << self.body;
    }


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

      scheme res = *this;      
      
      const vars_type all = vars();

      // copy free variables (i.e. instantiated at a larger depth)
      auto out = std::inserter(res.vars, res.vars.begin());
      std::copy_if(all.begin(), all.end(), out, [depth](const ref<variable>& v) {
          return v->depth >= depth;
        });
      
      return res;
    }


    constructor::table_type constructor::table;
    
    using special_type = mono (*)(const ref<context>& ctx, const sexpr::list& terms);
    
    static std::map<symbol, special_type> special = {

    };

    const constructor func_ctor("->", 2);
    
    static mono check_expr(const ref<context>& ctx, const sexpr& e);
    static void unify(const mono& lhs, const mono& rhs);

    
    struct unification_error : type_error {
      unification_error(const mono& lhs, const mono& rhs) 
        : type_error("unification error") { }
    };

    using uf_type = union_find<mono>;

    
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
    



    
    
    struct unify_visitor {

      bool call_reverse = true;
      
      template<class T>
      void operator()(const T& self, const mono& rhs, uf_type& uf) const {
        // std::clog << "unifying: " << self << " and: " << rhs << std::endl;
        
        // double dispatch
        if( call_reverse ) {
          return rhs.apply( unify_visitor{false}, self, uf);
        } else {
          throw unification_error(self, rhs);
        }
      }
      
      void operator()(const ref<variable>& self, const mono& rhs, uf_type& uf) const {
        // rhs.apply(occurs_check(), self);

        mono s = uf.find(self);
        mono r = uf.find(rhs);
        
        uf.link(s, r);
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


    static union_find<mono> uf;    

    static void unify(const mono& lhs, const mono& rhs) {
      lhs.apply( unify_visitor(), rhs, uf );
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
            return application(func_ctor, {lhs, result_type});
          }
          
          return application(func_ctor, {lhs, rhs.get<application>()});
        }).get<application>();
      
      // TODO + uf?
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
      return e.map<mono>(expr_visitor(), ctx);
    }
    

    scheme check(const ref<context>& ctx, const sexpr& e) {
      // TODO check toplevel
      return check_expr(ctx, e).map<mono>(nice(), uf).generalize();
    }



    application operator>>=(const mono& lhs, const mono& rhs) {
      return application(func_ctor, {lhs, rhs});
    }
    
  }

}
