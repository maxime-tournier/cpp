#ifndef SLIP_AST_HPP
#define SLIP_AST_HPP

#include "list.hpp"
#include "symbol.hpp"
#include "sexpr.hpp"


// TODO repr ?

namespace slip {

  
  namespace kw {
    const extern symbol def, lambda, cond, wildcard;
    const extern symbol seq, var, pure, run;
    
    const extern symbol record;
    
    const extern symbol quote, unquote, quasiquote;
    
    const extern symbol type;

    const extern symbol ref, get, set;
    
    // const extern symbol ref, getref, setref;
  }

  
  namespace ast {
    
    struct expr;

    template<class T> struct literal;
    struct variable;
    struct lambda;
    struct application;
    struct definition;
    struct sequence;
    struct condition;
    struct binding;
    struct escape;
    
    struct selection {
      selection(symbol label) : label(label) { }
      symbol label;
    };
    
    template<class T>
    struct literal {
      literal(const T& value = {}) : value(value) { }
      const T value;
    };

    struct variable {
      symbol name;
    };
    
    struct sequence {
      list<expr> items;
    };



    struct branch;

    struct condition{
      list<branch> branches;
    };


    struct row;

    struct record {
      list<row> rows;
    };
    

    struct expr : variant< literal<unit>,
                           literal<boolean>,
                           literal<integer>,
                           literal<real>,
                           literal< ref<string> >,
                           variable,
                           lambda,
                           ref<application>,
                           ref<definition>,
                           ref<binding>,
                           sequence,
                           condition,
                           selection,
                           record> {
      
      using expr::variant::variant;

      using list = slip::list<expr>;

    };

    sexpr repr(const expr& self);
    std::ostream& operator<<(std::ostream& out, const expr& self);
    

    struct type;

    struct type_constructor {
      symbol name;
    };

    struct type_variable {
      symbol name;
    };

    struct type_application {
      type_constructor type;
      list<ast::type> args;
    };
    
    struct type : variant< type_constructor, type_variable, type_application > {
      using type::variant::variant;

      using list = slip::list<type>;
    };

    sexpr repr(const type& self);    
    
    
    struct lambda {

      struct typed {
        symbol name;
        struct ast::type type;
      };

      struct arg : variant<symbol, typed> {
        using arg::variant::variant;

        symbol name() const {
          if( is<symbol>() ) return get<symbol>();
          else return get<typed>().name;
        }
      };
      
      lambda(const list<arg>& args, const expr& body)
        : args(args), body(body) { }

      lambda(const lambda&) = default;
      
      list<arg> args;
      expr body;
    };

    
    struct application {
      application(const expr& func, const list<expr>& args)
        : func(func), args(args) { }
        
      const expr func;
      const list<expr> args;
    };



    // let bindings
    struct definition {
      definition(const symbol& id, const expr& value)
        : id(id),
          value(value) { }
        
      const symbol id;
      const expr value;
    };

    
    // monadic binding in a sequence
    struct binding {
      binding(const symbol& id, const expr& value)
        : id(id),
          value(value) { } 
      
      const symbol id;
      const expr value;
    };


    struct branch {
      branch(const expr& test, const expr& value)
        : test(test), value(value) { }
      
      expr test, value;
    };


    struct row {
      row(const symbol& label, const expr& value)
        : label(label), value(value) { }
      
      symbol label;
      expr value;
    };
    
    
    // TODO type, etc
    struct toplevel : variant<expr> {
      using toplevel::variant::variant;
    };
    
    std::ostream& operator<<(std::ostream& out, const toplevel& self);    
    
    sexpr repr(const toplevel& self);
    
    toplevel check_toplevel(const sexpr& e);
    
  }
  
}



#endif
