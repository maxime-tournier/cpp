#include "ast.hpp"

#include <map>

namespace slip {


  namespace kw {
    const symbol def = "def",
      lambda = "lambda",
      seq = "do",
      cond = "cond",
      wildcard = "_",

      quote = "quote",
      quasiquote = "quasiquote",
      unquote = "unquote";

    const symbol record = "record";
    const symbol module = "module";    
    
    const symbol var = "var", pure = "pure", run = "run";
    const symbol ref = "ref", get = "get", set = "set";
  }

  
  namespace ast {


    struct repr_visitor {

      using value_type = sexpr;
      
      template<class T>
      sexpr operator()(const literal<T>& self) const {
        return self.value;
      }

      sexpr operator()(const literal<string>& self) const {
        return make_ref<string>(self.value);
      }

      sexpr operator()(const variable& self) const {
        return self.name;
      }


      sexpr operator()(const ast::type::list& self) const {
        return map(self, [](const ast::type& e) { return repr(e); });
      }

      
      sexpr operator()(const lambda::typed& self) const {
        return self.name >>= repr(self.type) >>= sexpr::list();
      }


      sexpr operator()(const lambda& self) const {
        return kw::lambda
          >>= map(self.args, [this](const lambda::arg& s) -> sexpr {
              return s.apply( repr_visitor() );
            })
          >>= repr(self.body)
          >>= sexpr::list();
      }

      sexpr operator()(const definition& self) const {
        return kw::def
          >>= self.id
          >>= repr(self.value)
          >>= sexpr::list();
      }


      sexpr operator()(const binding& self) const {
        return kw::var
          >>= self.id
          >>= repr(self.value)
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


      sexpr operator()(const application& self) const {
        return repr(self.func) >>= map(self.args, [](const expr& e) {
            return repr(e);
          });
      }

      sexpr operator()(const selection& self) const {
        return symbol("@" + self.label.str());
      }

      sexpr operator()(const record& self) const {
        return symbol(kw::record) >>= map(self.rows, [](const record::row& r) -> sexpr {
            return r.label >>= repr(r.value) >>= sexpr::list();
          });
      }

      sexpr operator()(const type_variable& self) const { return self.name; }
      sexpr operator()(const type_constructor& self) const { return self.name; }      

      sexpr operator()(const type_application& self) const {
        return repr(self.type) >>= map(self.args, [](const ast::type& t) { return repr(t); } );
      }

      
      
      
      // template<class T>
      // sexpr operator()(const T& self) const {
      //   throw error(std::string("repr unimplemented for: ") + typeid(T).name() );
      // }
      
    };

    
    sexpr repr(const expr& self) {
      return self.apply( repr_visitor() );
    }

    sexpr repr(const type& self) {
      return self.apply( repr_visitor() );
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
    

    static expr check_variable(const symbol& s) {
      if(s.str()[0] == '\'') throw syntax_error("`symbol`");
      return ast::variable{s};
    }
    
    static type_constructor check_type_constructor(const sexpr& e) {
      if(!e.is<symbol>()) throw syntax_error("`symbol`");
      const symbol s = e.get<symbol>();
      
      if(s.str()[0] == '\'') throw syntax_error("`symbol`");
      
      return {s};
    }

    static type_variable check_type_variable(const symbol& s) {
      if(s.str()[0] != '\'' || s.str().size() < 2) throw syntax_error("`'symbol`");

      return {s};
    }

    
    static type check_type(const sexpr& e) {
      struct error {};

      try {
        if(e.is<symbol>())  {
          const symbol s = e.get<symbol>();
          
          if(s.str()[0] == '\'') return check_type_variable(s);
          else return check_type_constructor(s);
        }

        if(!e.is<sexpr::list>()) throw error();

        // type constructor
        const sexpr::list& arg = e.get<sexpr::list>();      
        if(size(arg) < 2) throw error(); // or is it not?

        return ast::type_application{ check_type_constructor(arg->head), map(arg->tail, &check_type) };
        
      } catch( error& ) {
        throw syntax_error("type: `symbol` | `'symbol` | (`symbol` `type`...)");
      }
    }

    static lambda::arg check_lambda_arg(const sexpr& e) {
      struct error { };

      try {
        if(e.is<symbol>()) return e.get<symbol>();
        
        if(!e.is<sexpr::list>()) throw error();

        const sexpr::list& arg = e.get<sexpr::list>();
        if( size(arg) != 2) throw error();

        if( !arg->head.is<symbol>() ) throw error();

        return lambda::typed{ arg->head.get<symbol>(), check_type(arg->tail->head) };            
        
      } catch(error& ) {
        throw syntax_error("typed arg: `symbol` | (`symbol` `type`)");
      }
    }
    
    static expr check_lambda(const sexpr::list& args) {
      struct error { };

      try {
        if(size(args) != 2 || !args->head.is<sexpr::list>() ) throw error();
        
        list<lambda::arg> vars =
          map(args->head.get<sexpr::list>(), [](const sexpr& e) {
              return check_lambda_arg(e);
            });

        // fix nullary functions
        // TODO make them unit here?
        if(!vars) vars = kw::wildcard >>= vars;
        
        return lambda(vars, check_expr(args->tail->head));
        
      } catch( error& ) {
        throw syntax_error("(lambda (`arg`...) `expr`)");
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
      
      return application{ check_expr(self->head), args};
    }
    
    
    static expr check_definition(const sexpr::list& items) {
      struct fail { };
      
      try {
        
        if( size(items) != 2 ) throw fail();
        if( !items->head.is<symbol>() ) throw fail();

        return definition{items->head.get<symbol>(),
            check_expr(items->tail->head)};
        
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

              return branch{check_expr(lst->head), check_expr(lst->tail->head)};
            }) };

        return res;
        
      } catch( fail ) {
        throw syntax_error("(cond (`expr` `expr`)...)");
      }
      
    }

    

    static expr check_record(const sexpr::list& items) {
      struct fail { };
      try{ 
        const record result = { map(items, [](const sexpr& e) -> record::row {
            
            if(!e.is<sexpr::list>() ) throw fail();
            const sexpr::list& lst = e.get<sexpr::list>();
            
            if(size(lst) != 2) throw fail();
            if(!lst->head.is<symbol>()) throw fail();
            
            return {lst->head.get<symbol>(), check_expr(lst->tail->head)};
          })
        };
        
        return result;
      } catch (fail ) {
        throw syntax_error("(record (`symbol` `expr`)...)");
      }
    };
    
    
    struct expr_visitor {
      using value_type = expr;
      
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

        const std::string& id = self.str();
        
        if(id[0] == '@') {
          const std::string label(id.data() + 1, id.data() + id.size());
          return selection(label);
        }
        
        
        return check_variable(self);
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
      return e.apply(expr_visitor());
    }
    


    static expr check_binding(const sexpr::list& items) {
      struct fail { };
      
      try {
        
        if( size(items) != 2 ) throw fail();
        if( !items->head.is<symbol>() ) throw fail();

        return binding{items->head.get<symbol>(), check_expr(items->tail->head)};
      } catch( fail& ) {
        throw syntax_error("(var `symbol` `expr`)");
      }
    }
    

    struct toplevel_visitor : expr_visitor {
      using value_type = toplevel;
      
      template<class T>
      toplevel operator()(const T& self) const {
        return expr_visitor::operator()(self);
      }
      
    };

    
    toplevel check_toplevel(const sexpr& e) {
      return e.apply(toplevel_visitor());
    }

    
    std::ostream& operator<<(std::ostream& out, const toplevel& self) {
      return out << self.get<expr>();
    }

    sexpr repr(const toplevel& self) {
      return repr(self.get<expr>());
    }
    
  }
  
}
