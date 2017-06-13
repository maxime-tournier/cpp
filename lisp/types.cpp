#include "types.hpp"

#include <algorithm>

#include "syntax.hpp"

namespace lisp {

  namespace types {

    template<class T>
    struct traits;


    template<>
    struct traits< unit > {
      static symbol name() { return "unit"; }
    };

    
    template<>
    struct traits< boolean > {
      static symbol name() { return "boolean"; }
    };


    template<>
    struct traits< integer > {
      static symbol name() { return "integer"; }
    };

    template<>
    struct traits< real > {
      static symbol name() { return "real"; }
    };


    template<>
    struct traits< ref<string> > {
      static symbol name() { return "string"; }
    };

    template<>
    struct traits< symbol > {
      static symbol name() { return "symbol"; }
    };
    

    struct ostream_visitor {

      void operator()(const constant& self, std::ostream& out) const {
        out << self.name;
      }

      void operator()(const application& self, std::ostream& out) const {
        out << self.ctor.name();

        if(self.args.empty()) return;
        out << ' ';

        out << make_list<mono>(self.args.begin(), self.args.end());
      }


      void operator()(const ref<variable>& self, std::ostream& out) const {
        // TODO we need some context
        out << "#<var: " << self << ">";
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

    static const constructor func("->", 2);
    

    static mono check_expr(const ref<context>& ctx, const sexpr& e);

    static void unify(const mono& lhs, const mono& rhs) {
      throw error("unify: not implemented");
    }
    

    static mono check_app(const ref<context>& ctx, const sexpr::list& self) {
      assert(self);

      const mono func_type = check_expr(ctx, self->head);

      const list<mono> args_type = map(self->tail, [&ctx](const sexpr& e) -> mono{
          return check_expr(ctx, e);
        });

      const ref<variable> result_type = ctx->fresh();
      
      static const application init( constructor("__init__", 0), {});
      
      const mono app_type = foldr(init, args_type, [&result_type](const mono& lhs, const application& rhs) {
          if(&rhs == &init) {
            return application(func, {lhs, result_type});
          }
          
          return application(func, {lhs, rhs});
        });

      // TODO + uf
      unify(func_type, app_type);

      return result_type;
    }


    mono scheme::instantiate(std::size_t depth) const {
      throw error("instantiate not implemented");
    }
    
    
    struct expr_visitor {
      
      // literals
      template<class T>
      mono operator()(const T& self, const ref<context>& ctx) const {
        return constant( traits<T>::name() );
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
      return check_expr(ctx, e).generalize();
    }

  }

}
