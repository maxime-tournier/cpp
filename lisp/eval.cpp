#include "eval.hpp"

#include "syntax.hpp"
#include <sstream>

namespace lisp {

  unbound_variable::unbound_variable(const symbol& s) 
    : error("unbound variable: " + s.name()) { }
  

  lambda::lambda(const ref<context>& ctx,
                 args_type&& args,
                 const sexpr& body)
    : ctx(ctx),
      args(std::move(args)),
      body(body) {

  }

  struct ostream_visitor {

    void operator()(const value::list& self, std::ostream& out) const {
      out << '(' << self << ')';
    }
    
    void operator()(const symbol& self, std::ostream& out) const {
      out << self.name();
    }

    void operator()(const ref<string>& self, std::ostream& out) const {
      out << *self;
    }

    
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }
    
  };

  std::ostream& operator<<(std::ostream& out, const value& self) {
    self.apply( ostream_visitor(), out );
    return out;
  }
  
  
  context& context::operator()(const symbol& name, const value& expr) {
    locals.emplace( std::make_pair(name, expr) );
    return *this;
  }


  value& context::find(const symbol& name){
    auto it = locals.find(name);
    if(it != locals.end()) return it->second;
    if(parent) return parent->find(name);
    throw unbound_variable(name);
  }  
  
  namespace special {
    using type = value (*)(const ref<context>&, const sexpr::list& args);
    
    static value lambda(const ref<context>& ctx, const sexpr::list& args) {
      const sexpr::list& var_list = args->head.get<sexpr::list>();
      const sexpr& body = args->tail->head;
      
      std::vector<symbol> vars;
      
      for(const sexpr& v : var_list) {
        vars.emplace_back( v.get<symbol>() );
      }
      
      return make_ref<lisp::lambda>(ctx, std::move(vars), body);
    }

    
    static value def(const ref<context>& ctx, const sexpr::list& args) {
      sexpr::list curr = args;

      const symbol name = curr->head.get<symbol>();

      curr = curr->tail;
      const value val = eval(ctx, curr->head);

      // TODO semantic: do we allow duplicate symbols ?
      auto res = ctx->locals.emplace( std::make_pair(name, val) );
      if(!res.second) throw error(name.name() + " already defined");
      
      return value::list();
    }
  

    static value cond(const ref<context>& ctx, const sexpr::list& args) {
      for(const sexpr& x : args) {
        const sexpr::list& term = x.get<sexpr::list>();
        
        if( eval(ctx, term->head) ) {
          return eval(ctx, term->tail->head);
        }
        
      }
      
      return value::list();
    }
    
 
    static value quote(const ref<context>& ctx, const sexpr::list& args) {
      return args->head;
    }
    
    static value seq(const ref<context>& ctx, const sexpr::list& args) {

      value res = value::list();
      for(const sexpr& x : args) {
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

    
    value operator()(const sexpr::list& self, const ref<context>& ctx) const {
      if(!self) throw error("empty list in application");

      const sexpr& first = self->head;
      
      // special forms dispatch
      if( first.is<symbol>() ) {
        auto it = special::table.find( first.get<symbol>() );
        if(it != special::table.end()) {
          return it->second(ctx, self->tail);
        }
      }
      

      // applicatives
      const value app = eval(ctx, first);

      const std::size_t start = stack.size();
      // TODO use VLA here instead?

      for(const sexpr& x : self->tail) {
        stack.emplace_back( eval(ctx, x) );
      }
      
      const value res = apply(app, stack.data() + start, stack.data() + stack.size());
      stack.resize(start, value::list());
      
      return res;
    }
    
    
    template<class T>
    value operator()(const T& self, const ref<context>& ctx) const {
      return self; 
    }
    
  };

  // not quite sure where does this belong
  std::vector<value> eval_visitor::stack;
  
  value eval(const ref<context>& ctx, const sexpr& expr) {
    return expr.map<value>( eval_visitor(), ctx );
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
    return app.map<value>(apply_visitor(), first, last);
  }
  

  
}
