#include "syntax.hpp"
#include "eval.hpp"

#include <map>


namespace lisp {

  namespace kw {
    const symbol def = "def", lambda = "lambda", seq = "do", cond = "cond", quote = "quote";
  }

  
  namespace special {

    using type = sexpr::list (*)(const ref<context>&, const sexpr::list& args);
    
    static sexpr::list def(const ref<context>& ctx, const sexpr::list& args) {

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
        
        return name >>= body >>= sexpr::list();
        
      } catch( value::bad_cast& e) {
        fail();
      } catch( empty_list& e) {
        fail();
      }

      throw;
    }


    static sexpr::list quote(const ref<context>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(quote `expr`)");
      };
      
      sexpr::list curr = args;
      try {
        const sexpr& res = head(curr);

        curr = tail(curr);
        if(curr) fail();

        return res >>= sexpr::list();
        
      } catch( empty_list& e) {
        fail();
      }

      throw;
    }

    
    static sexpr::list lambda(const ref<context>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(lambda (`symbol`...) `expr`)");
      };
      
      sexpr::list curr = args;
      try {

        const sexpr::list vars = map(head(curr).cast<sexpr::list>(), [](const sexpr& x) {
            return x.cast<symbol>();
          });
        
        curr = tail(curr);
        const sexpr body = expand(ctx, head(curr));
        
        curr = tail(curr);
        if(curr) fail();

        return vars >>= body >>= sexpr::list();
      }
      catch( value::bad_cast& e) {
        fail();
      }
      catch( empty_list& e) {
        fail();
      }

      throw;
    }
    


    static sexpr::list seq(const ref<context>& ctx, const sexpr::list& args) {
      return map(args, [&](const sexpr& x) {
          return expand_seq(ctx, x);
        });
    }


    static sexpr::list cond(const ref<context>& ctx, const sexpr::list& args) {
      static const auto fail = [] {
        throw syntax_error("(cond (`expr` `expr`)...)");
      };
      try{
        return map(args, [&](const sexpr& e) -> sexpr {
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
      {kw::cond, cond}
    };

    static const std::set<symbol> reserved = {
      kw::seq,
      kw::lambda,
      kw::quote,
      kw::cond,
      kw::def
    };

  }

  
  struct expand_visitor {

    template<class T>
    sexpr operator()(const T& self, const ref<context>& ctx) const {
      return self;
    }

    sexpr operator()(const symbol& self, const ref<context>& ctx) const {
      if(special::reserved.find(self) != special::reserved.end()) {
        throw syntax_error(self.name() + " not allowed in this context");
      }
      
      return self; 
    }



    sexpr operator()(const sexpr::list& self, const ref<context>& ctx) const {

      if(self && head(self).is<symbol>()) {
        const symbol name = head(self).get<symbol>();

        auto it = special::table.find(name);
        if(it != special::table.end()) {
          return name >>= it->second(ctx, tail(self));
        }
        
        // TODO macros
      }

      return map(self, [&](const sexpr& e) {
          return expand(ctx, e);
        });
    }

  };



  struct expand_seq_visitor : expand_visitor {

    using expand_visitor::operator();
    
    sexpr operator()(const sexpr::list& self, const ref<context>& ctx) const {
      if(self && head(self).is<symbol>() ) {
        const symbol name = head(self).get<symbol>();

        if(name == kw::def) {
          return name >>= special::def(ctx, tail(self));
        }
      }
      
      return expand_visitor::operator()(self, ctx);
    };
    
  };
  
  
  sexpr expand(const ref<context>& ctx, const sexpr& expr) {
    return expr.map<sexpr>(expand_visitor(), ctx);
  }

  sexpr expand_seq(const ref<context>& ctx, const sexpr& expr) {
    return expr.map<sexpr>(expand_seq_visitor(), ctx);
  }
  
}
