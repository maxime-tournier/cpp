#include "eval.hpp"
#include "syntax.hpp"

namespace lisp {

  unbound_variable::unbound_variable(const symbol& s) 
    : error("unbound variable: " + s.name()) { }
  

  namespace special {
    using type = value (*)(const ref<context>&, const list& args);
    
    static value lambda(const ref<context>& ctx, const list& args) {
      
      try {
        list params;
        const list& rest = unpack(args, &params);

        std::vector<symbol> vars;
        
        symbol s;
        while(params) {
          params = unpack(params, &s);
          vars.push_back(s);
        }
        
        return make_ref<lisp::lambda>(ctx, std::move(vars), head(rest));
      } catch(empty_list& e) {
        throw syntax_error("lambda");        
      } catch(value::bad_cast& e) {
        throw syntax_error("symbol list expected for function parameters");
      } 

    }

    static value def(const ref<context>& ctx, const list& args) {
      list curr = args;
      try{
        const symbol name = head(curr).get<symbol>();
        
        const value val = eval(ctx, head(curr));
        auto res = ctx->locals.emplace( std::make_pair(name, val) );
        if(!res.second) res.first->second = val;
        
        // TODO nil?
        return nil;
      } catch( value::bad_cast& e) {
        throw syntax_error("symbol expected");
      } catch( empty_list& e) {
        throw syntax_error("def");
      }
       
    }
  

    static value cond(const ref<context>& ctx, const list& args) {
      try {
        for(const value& x : args) {
          const list& term = x.cast<list>();
 
          if( eval(ctx, head(term)) ) {
            return eval(ctx, head(tail(term)));
          }
        }
      } catch(value::bad_cast& e) {
        throw syntax_error("list of pairs expected");
      } catch( empty_list& e ){
        throw syntax_error("cond");
      } 
      
      return nil;
    }
    
 
    static value quote(const ref<context>& ctx, const list& args) {
      try {
        return head(args);
      } catch( empty_list& e ) {
        throw syntax_error("quote");
      }
    }


    // special forms
    static const std::map<symbol, type> table = {
      {"lambda", lambda},
      {"def", def},
      {"cond", cond},
      {"quote", quote}
    };

    static const auto table_end = table.end();
  }
  
  struct eval_visitor {

    static std::vector< value > stack;
    
    value operator()(const symbol& self, const ref<context>& ctx) const {
      return ctx->find(self);
    }

    value operator()(const list& self, const ref<context>& ctx) const {
      if(!self) throw error("empty list in application");

      const value& first = self->head;

      // special forms
      if(first.is<symbol>()) {
        auto it = special::table.find(first.get<symbol>());
        if(it != special::table_end) return it->second(ctx, self->tail);
      }
      

      // applicatives
      const value app = eval(ctx, first);

      const std::size_t start = stack.size();
      
      for(const value& x : self->tail) {
        stack.emplace_back( eval(ctx, x) );
      }
      
      const value res = apply(app, stack.data() + start, stack.data() + stack.size());
      stack.resize(start, nil);

      return res;
    }
    
    
    template<class T>
    value operator()(const T& self, const ref<context>& ctx) const {
      return self; 
    }
    
  };


  std::vector<value> eval_visitor::stack;
  
  value eval(const ref<context>& ctx, const value& expr) {
    return expr.map( eval_visitor(), ctx );
  }


  struct apply_visitor {

    value operator()(const ref<lambda>& self, const value* first, const value* last) {

      struct iterator {
        lambda::args_type::iterator it_name;
        const value* it_arg;

        iterator& operator++() {
          ++it_name, ++it_arg;
          return *this;
        }

        std::pair<symbol, const value&> operator*() const {
          return {*it_name, *it_arg};
        }
        
        bool operator!=(const iterator& other) const {
          const bool cond_name = it_name != other.it_name;
          const bool cond_arg = it_arg != other.it_arg;

          if(cond_name ^ cond_arg) throw error("arg count");

          return cond_name || cond_arg;
        }
        
      };

      iterator pair_first = {self->args.begin(), first};
      iterator pair_last = {self->args.end(), last};

      ref<context> sub = extend(self->ctx, pair_first, pair_last);
      return eval(sub, self->body);
    }

    value operator()(const builtin& self, const value* first, const value* last) {
      return self(first, last);      
    }
    

    template<class T>
    value operator()(const T& self, const value* first, const value* last) {
      throw error("cannot apply");
    }
    
  };

  
  value apply(const value& app, const value* first, const value* last) {
    return app.map(apply_visitor(), first, last);
  }
  

  
}
