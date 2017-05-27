#include "syntax.hpp"
#include "eval.hpp"

#include <map>


namespace lisp {

  syntax_error::syntax_error(const std::string& s)  
    : error("syntax error: " + s) { }


  
  namespace special {

    using type = value (*)(const ref<context>&, const list& args);


    static value def(const ref<context>& ctx, const list& args) {
      list curr = args;
      try{
        symbol name;
        curr = unpack(curr, &name);

        const value body = expand(ctx, head(curr));

        curr = tail(curr);
        if(curr) throw empty_list();
        
        return symbol("def") >>= name >>= body >>= nil;
        
      } catch( value::bad_cast& e) {
        throw syntax_error("symbol expected");
      } catch( empty_list& e) {
        throw syntax_error("def");
      }
      
    }


    
    static const std::map<symbol, type> table = {
      {"def", def}
      
    };

  }

  
  struct expand_visitor {

    template<class T>
    value operator()(const T& self, const ref<context>& ctx) const {
      return self;
    }

    value operator()(const symbol& self, const ref<context>& ctx) const {
      if(special::table.find(self) != special::table.end()) {
        throw syntax_error("nope");
      }
      
      return self;
    }



    value operator()(const list& self, const ref<context>& ctx) const {
      
      if(self && head(self).is<symbol>()) {
        const symbol name = head(self).get<symbol>();

        auto it = special::table.find(name);
        if(it != special::table.end()) {
          return it->second(ctx, tail(self));
        }

        // TODO macros
      }

      return self;
    }

  };
  
  value expand(const ref<context>& ctx, const value& expr) {
    return expr.map(expand_visitor(), ctx);
  }

}
