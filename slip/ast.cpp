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
    const symbol module = "module", export_ = "export";    
    
    const symbol var = "var", pure = "pure", run = "run";
    const symbol ref = "ref", get = "get", set = "set";
  }

  
  namespace ast {

    template<class T>
    static bool starts_with(const sexpr::list& x, const T& what) {
      return x && x->head.is<T>() && x->head.get<T>() == what;
    }
    

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
        return self.ctor >>= self.arg >>= sexpr::list();
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
      sexpr operator()(const type_constant& self) const { return self.name; }      

      sexpr operator()(const type_application& self) const {
        return repr(self.ctor) >>= map(self.args, [](const ast::type& t) { 
            return repr(t);
          });
      }

      sexpr operator()(const export_& self) const {
        return kw::export_ >>= self.name >>= repr(self.value) >>= sexpr::list();
      }

        
      

      sexpr operator()(const expr& self) const {
        return self.apply(repr_visitor());
      }


      sexpr operator()(const module& self) const {
        return kw::module >>= (repr(self.ctor) >>= map(self.args, repr_visitor()) )
          >>= map(self.rows, [](const module::row& row) -> sexpr {
            return row.name >>= repr(row.type) >>= sexpr::list();
          });
      }
      
      
      template<class T>
      sexpr operator()(const T& self) const {
        throw error(std::string("repr unimplemented for: ") + typeid(T).name() );
      }
      
    };

    
    sexpr repr(const expr& self) {
      return self.apply( repr_visitor() );
    }

    sexpr repr(const type& self) {
      return self.apply( repr_visitor() );
    }

    
    

    static expr check_expr(const sexpr& e);
    

    using special_type = expr (const sexpr::list& e);

    static special_type check_lambda,
      check_application,
      check_definition,
      check_condition,
      check_sequence,
      check_binding,
      check_record,
      check_export
      ;
    
    static expr check_binding(const sexpr::list&);

    static const std::map< symbol, special_type* > special = {
      {kw::lambda, check_lambda},
      {kw::def, check_definition},
      {kw::cond, check_condition},
      {kw::var, check_binding},
      {kw::seq, check_sequence},
      {kw::record, check_record},
      {kw::export_, check_export},            
    };
    

    static expr check_variable(const symbol& s) {
      if(s.str()[0] == '\'') throw syntax_error("`symbol`");
      return ast::variable{s};
    }
    
    static type_constant check_type_constructor(const sexpr& e) {
      static const syntax_error error("tycon: `symbol`");

      try {
        const symbol s = e.get<symbol>();

        // exclude type variable
        if(s.str()[0] == '\'') {
          throw error;
        }
        return {s};
        
      } catch( std::bad_cast ) {
        throw error;
      }
    }

    static type_variable check_type_variable(const symbol& s) {
      if(s.str()[0] != '\'' || s.str().size() < 2) {
        throw syntax_error("tyvar: `'symbol`");
      }
      return {s};
    }

    
    static type check_type(const sexpr& e) {
      static const syntax_error error("type: `symbol` | `'symbol` | (`symbol` `type` `type`...)");
      
      try {
        if(e.is<symbol>())  {
          const symbol& s = e.get<symbol>();
          
          if(s.str()[0] == '\'') return check_type_variable(s);
          else return check_type_constructor(s);
        }

        // type constructor
        const sexpr::list& arg = e.get<sexpr::list>();      
        if(size(arg) < 2) throw error; // TODO or is it not? do we want nullary type
                                       // applications?

        return ast::type_application{ check_type(arg->head),
            map(arg->tail, &check_type) };
        
      } catch( std::bad_cast ) {
        throw error;
      }
    }

    
    static lambda::arg check_lambda_arg(const sexpr& e) {
      static const syntax_error error("typed arg: `symbol` | (`symbol` `symbol`)");
      
      try {
        if(e.is<symbol>()) return e.get<symbol>();
        const sexpr::list& arg = e.get<sexpr::list>();
        
        if( size(arg) != 2) throw error;
        
        return lambda::typed{ at(arg, 0).get<symbol>(), at(arg, 1).get<symbol>()};
        
      } catch(std::bad_cast) {
        throw error;
      }
    }
    
    
    static expr check_lambda(const sexpr::list& args) {
      static const syntax_error error("(lambda (`arg`...) `expr`)");
      try {
        if(size(args) != 2) throw error;
        
        list<lambda::arg> vars =
          map(args->head.get<sexpr::list>(), [](const sexpr& e) {
              return check_lambda_arg(e);
            });

        // fix nullary functions
        // TODO make them unit here?
        if(!vars) vars = kw::wildcard >>= vars;
        
        return lambda(vars, check_expr(args->tail->head));
        
      } catch(std::bad_cast) {
        throw error;
      }
      
    }
    

    static expr check_application(const sexpr::list& self) {
      static const syntax_error error("empty list in application");
      if(!self) throw error;
      
      list<expr> args = map(self->tail, [](const sexpr& e) {
          return check_expr(e);
        });
      
      // fix nullary applications
      if(!args) args = literal<unit>() >>= args;
      
      return application{ check_expr(self->head), args};
    }
    
    
    static expr check_definition(const sexpr::list& items) {
      static const syntax_error error("(def `symbol` `expr`)");
      try {
        if( size(items) != 2 ) throw error;
        
        return definition{items->head.get<symbol>(),
            check_expr(items->tail->head)};
        
      } catch(std::bad_cast) {
        throw error;
      }
    }


    
    static expr check_sequence(const sexpr::list& items) {
      return sequence{map(items, [](const sexpr& e) {
            return check_expr(e);
          })};
    }

    static expr check_condition(const sexpr::list& items) {
      static const syntax_error error("(cond (`expr` `expr`)...)");
      
      try{
        list<branch> branches = map(items, [](const sexpr& e) -> branch {
            const sexpr::list& lst = e.get<sexpr::list>();
            if(size(lst) != 2) throw error;
            return branch{check_expr(lst->head), check_expr(lst->tail->head)};
          });

        // fixup missing else statement
        branches = foldr(list<branch>(), branches, [](const branch& lhs, const list<branch>& rhs) {

            // final "else" case is missing
            if(!rhs && (!lhs.test.is<literal<bool>>() || !lhs.test.get<literal<bool>>().value)) {
              const branch fix = {literal<boolean>{true}, sequence{}};              
              return lhs >>= fix >>= rhs;
            }

            return lhs >>= rhs;
          });
        
        return condition{branches};
        
      } catch(std::bad_cast) {
        throw error;
      }
      
    }

    

    static expr check_record(const sexpr::list& items) {
      static const syntax_error error("(record (`symbol` `expr`)...)");
      
      try{ 
        return record{ map(items, [](const sexpr& e) -> record::row {
              const sexpr::list& lst = e.get<sexpr::list>();
              
              if(size(lst) != 2) throw error;
              return {lst->head.get<symbol>(), check_expr(lst->tail->head)};
            })
            };
      } catch (std::bad_cast) {
        throw error;
      }
    };



    static expr check_export(const sexpr::list& items) {
      static const syntax_error error("(export `symbol` `expr`)");
      
      try{
        if(size(items) != 2) throw error;
        return export_{at(items, 0).get<symbol>(), check_expr(at(items, 1)) };
      } catch(std::bad_cast) {
        throw error;
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
      static const syntax_error error("(var `symbol` `expr`)");
      try {
        
        if( size(items) != 2 ) throw error;
        return binding{items->head.get<symbol>(), check_expr(items->tail->head)};
      } catch( std::bad_cast ) {
        throw error;
      }
    }


    static module check_module(const sexpr::list& items) {
      static const syntax_error error("(module (`symbol` `tyvars`...) "
                                      "(`symbol` `type`)...)");
      
      try {
        // TODO do we want empty modules?
        if(size(items) < 1) throw error;
        
        const ast::type type = check_type(items->head);

        struct check {
          using value_type = type_application;
          
          value_type operator()(const type_application& self) const { return self; }
          value_type operator()(const type_constant& self) const { return {self, {}};}
          value_type operator()(const type_variable& self) const { throw error; }
          
        };
        
        // normalize to a type application
        const type_application app = type.apply(check());

        // extract application func/args
        const type_constant ctor = app.ctor.get<type_constant>();
        const list<type_variable> args = map(app.args, [](const ast::type& t) {
            return t.get<type_variable>();
          });

        // make sure we don't have duplicates
        if( std::set<type_variable>( begin(args), end(args) ).size() < size(args) ) {
          throw syntax_error("duplicate type variable in arguments");
        }
        
        // extract rows
        const list<module::row> rows = map(items->tail, [](const sexpr& e) {
            const sexpr::list& self = e.get<sexpr::list>();
            if(size(self) != 2) throw error;
            
            return module::row{self->head.get<symbol>(), check_type(self->tail->head)};
          });

        return module{ctor, args, rows};
      } catch( std::bad_cast ) {
        throw error;
      }
    }

    struct toplevel_visitor : expr_visitor {
      using value_type = toplevel;

      toplevel operator()(const sexpr::list& self) const {
        if( starts_with(self, kw::module) ) {
          return check_module(self->tail);
        }
        
        return expr_visitor::operator()(self);
      }
      
      
      template<class T>
      toplevel operator()(const T& self) const {
        return expr_visitor::operator()(self);
      }
      
    };

    
    toplevel check_toplevel(const sexpr& e) {
      return e.apply(toplevel_visitor());
    }

    
    sexpr repr(const toplevel& self) {
      return self.apply( repr_visitor() );
    }
    
  }
  
}
