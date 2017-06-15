#include "syntax.hpp"
#include "eval.hpp"

#include <map>

#include <iostream>
#include <sstream>

namespace lisp {

  namespace kw {
    const symbol def = "def",
      lambda = "lambda",
      seq = "do",
      cond = "cond",
      quote = "quote",
      quasiquote = "quasiquote",
      unquote = "unquote",
      let = "let";
  }

  
  namespace special {

    using type = sexpr (*)(const ref<environment>&, const sexpr::list& args);
    
    static sexpr def(const ref<environment>& ctx, const sexpr::list& args) {

      static const auto fail = [] {
        throw syntax_error("(def `symbol` `expr`)");
      };
      
      sexpr::list curr = args;
      try{
        const symbol& name = head(curr).cast<symbol>();
        curr = tail(curr);
        
        const sexpr body = expand(ctx, head(curr));
        
        curr = tail(curr);
        if(curr) fail();
        
        return kw::def >>= name >>= body >>= sexpr::list();
        
      } catch( value::bad_cast& e) {
        fail();
      } catch( empty_list& e) {
        fail();
      }

      throw;
    }


    struct quasiquote_visitor {
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
            return e.map<sexpr>(quasiquote_visitor(), ctx);
          });
      }
      
    };
    
    static sexpr quasiquote(const ref<environment>& ctx, const sexpr::list& args) {
      if(!args || args->tail) throw syntax_error("(quasiquote `expr`)");
      
      return args->head.map<sexpr>(quasiquote_visitor(), ctx);
    }

    
    
    static sexpr quote(const ref<environment>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(quote `expr`)");
      };
      
      sexpr::list curr = args;
      try {
        const sexpr& res = head(curr);

        curr = tail(curr);
        if(curr) fail();

        return kw::quote >>= res >>= sexpr::list();
        
      } catch( empty_list& e) {
        fail();
      }

      throw;
    }

    
    static sexpr lambda(const ref<environment>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(lambda (`symbol`...) `expr`)");
      };
      
      sexpr::list curr = args;
      try {

        const sexpr::list vars = map(head(curr).cast<sexpr::list>(), [](const sexpr& x) -> sexpr {
            return x.cast<symbol>();
          });
        
        curr = tail(curr);
        const sexpr body = expand(ctx, head(curr));
        
        curr = tail(curr);
        if(curr) fail();

        return kw::lambda >>= vars >>= body >>= sexpr::list();
      }
      catch( value::bad_cast& e) {
        fail();
      }
      catch( empty_list& e) {
        fail();
      }

      throw;
    }
    


    static sexpr seq(const ref<environment>& ctx, const sexpr::list& args) {
      return kw::seq >>= map(args, [&ctx](const sexpr& x) {
          return expand_seq(ctx, x);
        });
    }


    static sexpr cond(const ref<environment>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(cond (`expr` `expr`)...)");
      };
      try{
        return kw::cond >>= map(args, [&ctx](const sexpr& e) -> sexpr {
            sexpr::list curr = e.cast<sexpr::list>();
            
            const sexpr test = expand(ctx, head(curr));
            curr = tail(curr);

            const sexpr value = expand(ctx, head(curr));
            curr = tail(curr);

            if(curr) fail();

            return test >>= value >>= sexpr::list();
          });
        
      } catch(empty_list& e){
        fail();
      } catch(value::bad_cast& e) {
        fail();
      }
      
      throw;
    }
    
    
    static const std::map<symbol, type> table = {
      {kw::seq, seq},
      {kw::lambda, lambda},
      {kw::quote, quote},
      {kw::cond, cond},
      {kw::quasiquote, quasiquote}
    };

    static const std::set<symbol> reserved = {
      kw::seq,
      kw::lambda,
      kw::quote,
      kw::cond,
      kw::def,
      kw::quasiquote,
      kw::unquote
    };

  }

  
  struct expand_visitor {

    template<class T>
    sexpr operator()(const T& self, const ref<environment>& ctx) const {
      return self;
    }

    sexpr operator()(const symbol& self, const ref<environment>& ctx) const {
      if(special::reserved.find(self) != special::reserved.end()) {
        std::stringstream ss;
        ss << '`' << self.name() << "` not allowed in this environment";
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
      if(self && head(self).is<symbol>() ) {
        const symbol name = head(self).get<symbol>();

        if(name == kw::def) {
          return special::def(ctx, tail(self));
        }
      }
      
      return expand_visitor::operator()(self, ctx);
    };
    
  };
  
  
  sexpr expand(const ref<environment>& ctx, const sexpr& expr) {
    
    const auto res = expr.map<sexpr>(expand_visitor(), ctx);
    // std::clog << "expand: " << expr << " -> " << res << std::endl;
    return res;
  }

  sexpr expand_seq(const ref<environment>& ctx, const sexpr& expr) {
    const auto res = expr.map<sexpr>(expand_seq_visitor(), ctx);
    // std::clog << "expand: " << expr << " -> " << res << std::endl;
    return res;
  }
  
}
