#include "syntax.hpp"
#include "eval.hpp"

#include <map>

#include <iostream>
#include <sstream>

#include "ast.hpp"

namespace slip {

  
  namespace special {

    using type = sexpr (*)(const ref<environment>&, const sexpr::list& args);
    
    static sexpr def(const ref<environment>& ctx, const sexpr::list& args) {

      static const auto fail = [] {
        throw syntax_error("(def `symbol` `expr`)");
      };
      
      sexpr::list curr = args;
      try{
        const symbol& name = head(curr).get<symbol>();
        curr = tail(curr);
        
        const sexpr body = expand(ctx, head(curr));
        
        curr = tail(curr);
        if(curr) fail();
        
        return kw::def >>= name >>= body >>= sexpr::list();
        
      } catch( std::bad_cast e) {
        fail();
      } catch( empty_list& e) {
        fail();
      }

      throw;
    }


    struct quasiquote_visitor {

      using value_type = sexpr;
      
      template<class T>
      sexpr operator()(const T& self, const ref<environment>& ctx) const {
        return kw::quote >>= self >>= sexpr::list();
      }

      sexpr operator()(const sexpr::list& self, const ref<environment>& ctx) const {
        if(self && self->head.is<symbol>() && self->head.get<symbol>() == kw::unquote) {

          if(!self->tail || self->tail->tail) {
            throw syntax_error("(unquote `expr`)");
          }

          // return unquoted/expanded
          return expand(ctx, self->tail->head);
        }

        // (list (quasiquote self)...)
        return symbol("list") >>= map(self, [&ctx](const sexpr& e) {
            return e.apply(quasiquote_visitor(), ctx);
          });
      }
      
    };

    
    static sexpr quasiquote(const ref<environment>& ctx, const sexpr::list& args) {
      if(size(args) != 1) {
        throw syntax_error("(quasiquote `expr`)");
      }
      
      return args->head.apply(quasiquote_visitor(), ctx);
    }


    // static sexpr let(const ref<environment>& ctx, const sexpr::list& args) {
    //   if(size(args) != 3 || !args->head.is<symbol>()) {
    //     throw syntax_error("(let `symbol` `expr` `expr`)");
    //   }
      
    //   return kw::let
    //     >>= args->head
    //     >>= expand(ctx, args->tail->head)
    //     >>= expand(ctx, args->tail->tail->head)
    //     >>= sexpr::list();
    // }
    
    
    
    static sexpr quote(const ref<environment>& ctx, const sexpr::list& args) {
        if(size(args) != 1) {
        throw syntax_error("(quote `expr`)");
      }

      return kw::quote >>= args;
    }

    
    static sexpr lambda(const ref<environment>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(lambda (`symbol`...) `expr`)");
      };

      if(size(args) != 2 || !args->head.is<sexpr::list>() ) {
        fail();
      }
      
      for(const sexpr& e : args->head.get<sexpr::list>()) {
        if(!e.is<symbol>()) fail();
      }

      // expand lambda body
      return kw::lambda >>= args->head >>= expand(ctx, args->tail->head) >>= sexpr::list();
    }
    


    static sexpr seq(const ref<environment>& ctx, const sexpr::list& args) {
      return kw::seq >>= map(args, [&ctx](const sexpr& x) {
          return expand_seq(ctx, x);
        });
    }


    struct type_visitor {
      struct error { };

      template<class T>
      void operator()(const T& self) const {
        throw error();
      }

      void operator()(const symbol& self) const { }

      void operator()(const sexpr::list& self) const {
        if(!self) throw error();
        if(!self->head.is<symbol>()) throw error();

        for(const sexpr& e : self->tail) {
          e.apply( type_visitor() );
        }
      }
      
      
      
    };
    

    static sexpr datatype(const ref<environment>& ctx, const sexpr::list& args) {
      // TODO macro-expand?
      static const auto fail = [] {
        throw syntax_error("(type (`symbol` `symbol`...) (`symbol` `type`...)...)");
      };

      if(!args || !args->head.is<sexpr::list>()) fail();

      for(const sexpr& e : args->head.get<sexpr::list>() ) {
        if(!e.is<symbol>()) fail();
      }

      try {
        for(const sexpr& e : args->tail) {
          e.apply(type_visitor());
        }
      } catch( type_visitor::error& e ) {
        fail();
      }

      return kw::type >>= args;
    }

    

    static sexpr cond(const ref<environment>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(cond (`expr` `expr`)...)");
      };
      try{
        return kw::cond >>= map(args, [&ctx](const sexpr& e) -> sexpr {
            sexpr::list curr = e.get<sexpr::list>();
            
            const sexpr test = expand(ctx, head(curr));
            curr = tail(curr);

            const sexpr value = expand(ctx, head(curr));
            curr = tail(curr);

            if(curr) fail();

            return test >>= value >>= sexpr::list();
          });
        
      } catch(empty_list& e){
        fail();
      } catch( std::bad_cast e) {
        fail();
      }
      
      throw;
    }
    
    
    static const std::map<symbol, type> table = {
      {kw::seq, seq},
      {kw::lambda, lambda},
      {kw::quote, quote},
      {kw::cond, cond},
      {kw::quasiquote, quasiquote},
    };

    static const std::set<symbol> reserved = {
      kw::seq,
      kw::lambda,
      kw::quote,
      kw::cond,
      kw::def,
      kw::quasiquote,
      kw::unquote,
      kw::type,
    };

  }

  
  struct expand_visitor {
    using value_type = sexpr;
    
    template<class T>
    sexpr operator()(const T& self, const ref<environment>& ctx) const {
      return self;
    }

    sexpr operator()(const symbol& self, const ref<environment>& ctx) const {
      if(special::reserved.find(self) != special::reserved.end()) {
        std::stringstream ss;
        ss << '`' << self.str() << "` not allowed in this context";
        throw syntax_error(ss.str());
      }

      // TODO where to put these?
      if(self == symbol("true")) return true;
      if(self == symbol("false")) return false;
      
      // if(self == symbol("unit")) return unit();            
      
      return self; 
    }



    sexpr operator()(const sexpr::list& self, const ref<environment>& ctx) const {

      if(!self) {
        throw syntax_error("empty list in application");
      }
      
      if(head(self).is<symbol>()) {
        const symbol name = head(self).get<symbol>();

        auto it = special::table.find(name);
        if(it != special::table.end()) {
          return it->second(ctx, tail(self));
        }
        
        // TODO macros
      }


      
      return map(self, [&ctx](const sexpr& e) {
          return expand(ctx, e);
        });
    }

    
    
  };



  struct expand_seq_visitor : expand_visitor {
    
    using expand_visitor::operator();
    
    sexpr operator()(const sexpr::list& self, const ref<environment>& ctx) const {
      if(self && self->head.is<symbol>() ) {
        const symbol& name = self->head.get<symbol>();
        
        if(name == kw::def) {
          return special::def(ctx, tail(self));
        }
      }
      
      return expand_visitor::operator()(self, ctx);
    };
    
  };
  
  
  sexpr expand(const ref<environment>& ctx, const sexpr& expr) {
    
    const auto res = expr.apply(expand_visitor(), ctx);
    // std::clog << "expand: " << expr << " -> " << res << std::endl;
    return res;
  }

  sexpr expand_seq(const ref<environment>& ctx, const sexpr& expr) {
    const auto res = expr.apply(expand_seq_visitor(), ctx);
    // std::clog << "expand: " << expr << " -> " << res << std::endl;
    return res;
  }



  struct expand_toplevel_visitor : expand_seq_visitor {

    using expand_seq_visitor::operator();

    sexpr operator()(const sexpr::list& self, const ref<environment>& ctx) const {
      if(self && self->head.is<symbol>() && self->head.get<symbol>() == kw::type) {
        return special::datatype(ctx, self->tail);
      }
      
      return expand_seq_visitor::operator()(self, ctx);
    }
  };
  
  
  sexpr expand_toplevel(const ref<environment>& ctx, const sexpr& expr) {
    const auto res = expr.apply(expand_toplevel_visitor(), ctx);
    return res;
  }

  
}
