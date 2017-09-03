#include "ast.hpp"

#include "syntax.hpp"

namespace slip {

  namespace ast {


    struct repr_visitor {
      
      template<class T>
      sexpr operator()(const literal<T>& self) const {
        return self.value;
      }

      sexpr operator()(const literal<string>& self) const {
        return make_ref<string>(self.value);
      }

      sexpr operator()(const symbol& self) const {
        return self;
      }
      
      sexpr operator()(const ref<lambda>& self) const {
        return kw::lambda
          >>= map(self->args, [](symbol s) -> sexpr { return s; })
          >>= repr(self->body)
          >>= sexpr::list();
      }

      sexpr operator()(const ref<definition>& self) const {
        return kw::def
          >>= self->id
          >>= repr(self->value)
          >>= sexpr::list();
      }


      sexpr operator()(const ref<binding>& self) const {
        return kw::var
          >>= self->id
          >>= repr(self->value)
          >>= sexpr::list();
      }


      sexpr operator()(const sequence& self) const {
        return kw::seq >>= map(self.items, [](const expr& e) {
            return repr(e);
          });
      }

      sexpr operator()(const condition& self) const {
        return kw::cond >>= map(self.branches, [](const branch& b) -> sexpr {
            return repr(b.test) >>= repr(b.value) >>= sexpr::list();
          });
      }


      sexpr operator()(const ref<application>& self) const {
        return repr(self->func) >>= map(self->args, [](const expr& e) {
            return repr(e);
          });
      }

      sexpr operator()(const selection& self) const {
        return symbol("@" + self.label.name());
      }

      sexpr operator()(const record& self) const {
        return symbol(kw::record) >>= map(self.rows, [](const row& r) -> sexpr {
            return r.label >>= repr(r.value) >>= sexpr::list();
          });
      }
      
      template<class T>
      sexpr operator()(const T& self) const {
        throw error("repr unimplemented");
      }
      
    };

    
    sexpr repr(const expr& self) {
      return self.map<sexpr>( repr_visitor() );
    }
    
    std::ostream& operator<<(std::ostream& out, const expr& self) {
      return out << repr(self);
    }
    
    

    static expr check_expr(const sexpr& e);
    

    using special_type = expr (const sexpr::list& e);

    static special_type check_lambda,
                    check_application,
                    check_definition,
                    check_condition,
                    check_sequence,
                    check_binding,
                    check_record;
    
    static expr check_binding(const sexpr::list&);

    static const std::map< symbol, special_type* > special = {
      {kw::lambda, check_lambda},
      {kw::def, check_definition},
      {kw::cond, check_condition},
      {kw::var, check_binding},
      {kw::seq, check_sequence},
      {kw::record, check_record},      
    };
    

    
    static expr check_lambda(const sexpr::list& args) {
      struct fail { };

      try {
        if(size(args) != 2 || !args->head.is<sexpr::list>() ) throw fail();
        
        list<symbol> vars =
          map(args->head.get<sexpr::list>(), [&](const sexpr& e) {
              if(!e.is<symbol>()) throw fail();
              return e.get<symbol>();
            });

        // fix nullary functions
        if(!vars) vars = kw::wildcard >>= vars;
        
        return make_ref<lambda>(vars, check_expr(args->tail->head));
        
      } catch( fail& ) {
        throw syntax_error("(lambda (`symbol`...) `expr`)");
      }
      
    }
    

    static expr check_application(const sexpr::list& self) {
      if(!self) {
        throw syntax_error("empty list in application");
      }

      list<expr> args = map(self->tail, [](const sexpr& e) {
          return check_expr(e);
        });

      // fix nullary applications
      if(!args) args = literal<unit>() >>= args;
      
      return make_ref<application>( check_expr(self->head), args);
    }
    
    
    static expr check_definition(const sexpr::list& items) {
      struct fail { };
      
      try {
        
        if( size(items) != 2 ) throw fail();
        if( !items->head.is<symbol>() ) throw fail();

        return make_ref<definition>(items->head.get<symbol>(),
                                    check_expr(items->tail->head));
        
      } catch( fail& ) {
        throw syntax_error("(def `symbol` `expr`)");
      }
    }


    
    static expr check_sequence(const sexpr::list& items) {
      const ast::sequence res = {map(items, [](const sexpr& e) {
            return check_expr(e);
          })};
      
      return res;
    }

    static expr check_condition(const sexpr::list& items) {
      struct fail {};
      
      try{

        const condition res = { map(items, [](const sexpr& e) {
              if(!e.is<sexpr::list>()) throw fail();
              const sexpr::list& lst = e.get<sexpr::list>();
              
              if(size(lst) != 2) throw fail();

              return branch(check_expr(lst->head), check_expr(lst->tail->head));
            }) };

        return res;
        
      } catch( fail ) {
        throw syntax_error("(cond (`expr` `expr`)...)");
      }
      
    }

    

    static expr check_record(const sexpr::list& items) {
      struct fail { };
      try{ 
        const record result = { map(items, [](const sexpr& e) -> row {
            
            if(!e.is<sexpr::list>() ) throw fail();
            const sexpr::list& lst = e.get<sexpr::list>();
            
            if(size(lst) != 2) throw fail();
            if(!lst->head.is<symbol>()) throw fail();
            
            return row(lst->head.get<symbol>(), check_expr(lst->tail->head));
          })
        };
        
        return result;
      } catch (fail ) {
        throw syntax_error("(record (`symbol` `expr`)...)");
      }
    };
    
    
    struct expr_visitor {

      template<class T>
      expr operator()(const T& self) const {
        return literal<T>{self};
      }
      
      expr operator()(const symbol& self) const {
        if(self == "true") {
          return literal<boolean>{true};
        }

        if(self == "false") {
          return literal<boolean>{false};
        }

        const std::string& id = self.name();
        
        if(id[0] == '@') {
          const std::string label(id.data() + 1, id.data() + id.size());
          return selection(label);
        }
        
        
        return self;
      }


      
      expr operator()(const sexpr::list& self) const {
        if(self && self->head.is<symbol>() ) {
          auto it = special.find(self->head.get<symbol>());
          if(it != special.end()) {
            return it->second(self->tail);
          }
        }

        return check_application(self);
      }      
      
    };
    

    static expr check_expr(const sexpr& e) {
      return e.map<expr>(expr_visitor());
    }
    


    static expr check_binding(const sexpr::list& items) {
      struct fail { };
      
      try {
        
        if( size(items) != 2 ) throw fail();
        if( !items->head.is<symbol>() ) throw fail();

        return make_ref<binding>(items->head.get<symbol>(), check_expr(items->tail->head));
      } catch( fail& ) {
        throw syntax_error("(var `symbol` `expr`)");
      }
    }
    

    struct toplevel_visitor : expr_visitor {

      template<class T>
      toplevel operator()(const T& self) const {
        return expr_visitor::operator()(self);
      }
      
    };

    
    toplevel check_toplevel(const sexpr& e) {
      return e.map<toplevel>(toplevel_visitor());
    }

    
    std::ostream& operator<<(std::ostream& out, const toplevel& self) {
      return out << self.get<expr>();
    }

    sexpr repr(const toplevel& self) {
      return repr(self.get<expr>());
    }
    
  }
  
}
