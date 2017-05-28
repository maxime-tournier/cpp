#include "syntax.hpp"
#include "eval.hpp"

#include <map>


namespace lisp {

  namespace kw {
    const symbol def = "def", lambda = "lambda", seq = "do", cond = "cond", quote = "quote";
  }

  
  namespace special {

    using type = list (*)(const ref<context>&, const list& args);

    
    static list def(const ref<context>& ctx, const list& args) {

      static const auto fail = [] {
        throw syntax_error("(def `symbol` `expr`)");
      };
      
      list curr = args;
      try{
        symbol name;
        curr = unpack(curr, &name);

        const value body = expand(ctx, head(curr));

        curr = tail(curr);
        if(curr) fail();
        

        
      } catch( value::bad_cast& e) {
        fail();
      } catch( empty_list& e) {
        fail();
      }

      return args;      
    }


    static list quote(const ref<context>& ctx, const list& args) {
      static const auto fail = [] {
        throw syntax_error("(quote `expr`)");
      };
      
      list curr = args;
      try {
        const value res = head(curr);

        curr = tail(curr);
        if(curr) fail();
      } catch( empty_list& e) {
        fail();
      }

      return args;
    }

    
    static list lambda(const ref<context>& ctx, const list& args) {
      static const auto fail = [] {
        throw syntax_error("(lambda (`symbol`...) `expr`)");
      };
      
      list curr = args;
      try {

        const list vars = map(head(curr).cast<list>(), [](const value& x) {
            return x.cast<symbol>();
          });
        
        curr = tail(curr);
        const value body = head(curr);

        curr = tail(curr);
        if(curr) fail();
        

      }
      catch( value::bad_cast& e) {
        fail();
      }
      catch( empty_list& e) {
        fail();
      }

      return args;      
    }
    


    static list seq(const ref<context>& ctx, const list& args) {
      return map(args, [&](const value& x) {
          return expand_seq(ctx, x);
        });
    }


    static list cond(const ref<context>& ctx, const list& args) {
      static const auto fail = [] {
        throw syntax_error("(cond (`expr` `expr`)...)");
      };
      try{
        for(const lisp::value& x : args) {
          list curr = x.cast<list>();

          curr = tail(curr);
          curr = tail(curr);

          if(curr) fail();
        }
      } catch(empty_list& e){
        fail();
      } catch(value::bad_cast& e) {
        fail();
      }
      
      return args;
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
    value operator()(const T& self, const ref<context>& ctx) const {
      return self;
    }

    value operator()(const symbol& self, const ref<context>& ctx) const {
      if(special::reserved.find(self) != special::reserved.end()) {
        throw syntax_error(self.name() + " not allowed in this context");
      }
      
      return self; 
    }



    value operator()(const list& self, const ref<context>& ctx) const {

      if(self && head(self).is<symbol>()) {
        const symbol name = head(self).get<symbol>();

        auto it = special::table.find(name);
        if(it != special::table.end()) {
          return name >>= it->second(ctx, tail(self));
        }
        
        // TODO macros
      }

      return map(self, [&](const value& e) {
          return expand(ctx, e);
        });
    }

  };



  struct expand_seq_visitor : expand_visitor {

    using expand_visitor::operator();
    
    value operator()(const list& self, const ref<context>& ctx) const {
      if(self && head(self).is<symbol>() ) {
        const symbol name = head(self).get<symbol>();

        if(name == kw::def) {
          return name >>= special::def(ctx, tail(self));
        }
      }
      
      return expand_visitor::operator()(self, ctx);
    };
    
  };
  
  
  value expand(const ref<context>& ctx, const value& expr) {
    return expr.map(expand_visitor(), ctx);
  }

  value expand_seq(const ref<context>& ctx, const value& expr) {
    return expr.map(expand_seq_visitor(), ctx);
  }
  
}
