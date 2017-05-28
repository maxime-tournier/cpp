#include "eval.hpp"

#include "syntax.hpp"
#include <sstream>

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

      const symbol name = curr->head.get<symbol>();

      curr = curr->tail;
      const value val = eval(ctx, curr->head);

      // TODO semantic: do we allow duplicate symbols ?
      auto res = ctx->locals.emplace( std::make_pair(name, val) );
      if(!res.second) throw error(name.name() + " already defined");
      
      return nil;
    }
  

    static value cond(const ref<context>& ctx, const list& args) {
      for(const value& x : args) {
        const list& term = x.get<list>();
 
        if( eval(ctx, term->head) ) {
          return eval(ctx, term->tail->head);
        }

      }
      
      return nil;
    }
    
 
    static value quote(const ref<context>& ctx, const list& args) {
      return args->head;
    }

    static value seq(const ref<context>& ctx, const list& args) {

      value res = nil;
      for(const value& x : args) {
        res = eval(ctx, x);
      }
      
      return res;
    }

    

    // special forms
    static const std::map<symbol, type> table = {
      {kw::lambda, lambda},
      {kw::def, def},
      {kw::cond, cond},
      {kw::quote, quote},
      {kw::seq, seq},      
    };

  }
  
  struct eval_visitor {

    static std::vector< value > stack;
    
    value operator()(const symbol& self, const ref<context>& ctx) const {
      if(special::table.find(self) != special::table.end()) {
        throw error(self.name() + " not allowed in this context");
      }
      return ctx->find(self);
    }

    value operator()(const list& self, const ref<context>& ctx) const {
      if(!self) throw error("empty list in application");

      const value& first = self->head;

      // special forms
      if(first.is<symbol>()) {
        auto it = special::table.find(first.get<symbol>());
        if(it != special::table.end()) {
          return it->second(ctx, self->tail);
        }
      }
      

      // applicatives
      const value app = eval(ctx, first);

      const std::size_t start = stack.size();
      // TODO use VLA here instead?
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

  // not quite sure where does this belong
  std::vector<value> eval_visitor::stack;
  
  value eval(const ref<context>& ctx, const value& expr) {
    return expr.map( eval_visitor(), ctx );
  }


  struct apply_visitor {

    value operator()(const ref<lambda>& self, const value* first, const value* last) {

      struct argc_error : error {
        argc_error() : error("argc") {} 
      };
      
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

          if(cond_name ^ cond_arg) throw  argc_error();

          return cond_name || cond_arg;
        }
        
      };

      iterator pair_first = {self->args.begin(), first};
      iterator pair_last = {self->args.end(), last};

      try {
        ref<context> sub = extend(self->ctx, pair_first, pair_last);
        return eval(sub, self->body);
      } catch( argc_error& e ) {
        std::stringstream ss;
        ss << "argument error, got " << last - first << ", expected: " << self->args.size();
        throw error(ss.str());
      }
    }

    value operator()(const builtin& self, const value* first, const value* last) {
      return self(first, last);      
    }
    

    template<class T>
    value operator()(const T& self, const value* first, const value* last) {
      // TODO
      throw error("cannot apply values of this type");
    }
    
  };

  
  value apply(const value& app, const value* first, const value* last) {
    return app.map(apply_visitor(), first, last);
  }
  

  
}
