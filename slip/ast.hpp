#ifndef SLIP_AST_HPP
#define SLIP_AST_HPP

#include "list.hpp"
#include "symbol.hpp"
#include "sexpr.hpp"


namespace slip {

  
  namespace kw {
    const extern symbol def, lambda, cond, wildcard;
    const extern symbol seq, var, pure, run;
    
    const extern symbol record;

    const extern symbol module, export_;
    
    const extern symbol quote, unquote, quasiquote;
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
    struct export_;
    
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

    struct record {
      
      struct row;

      list<row> rows;
    };
    

    struct expr : variant< literal<unit>,
                           literal<boolean>,
                           literal<integer>,
                           literal<real>,
                           literal< ref<string> >,
                           variable,
                           lambda,
                           application,
                           definition,
                           binding,
                           sequence,
                           condition,
                           selection,
                           record,
                           export_> {
      
      using expr::variant::variant;

      using list = slip::list<expr>;

    };

    sexpr repr(const expr& self);
    
    struct record::row {
      const symbol label;
      const expr value;
    };
    
    struct type;

    struct type_constant;
    struct type_variable;
    struct type_application;

    // TODO exclude naked type_constructors? (= nullary applications)
    struct type : variant< type_constant, type_variable, type_application > {
      using type::variant::variant;

      using list = slip::list<type>;
    };
    
    sexpr repr(const type& self);    

    struct type_constant {
      const symbol name;
    };
    
    struct type_variable {
      const symbol name;

      bool operator<(const type_variable& other) const {
        return name < other.name;
      }
    };
    
    struct type_application {
      type_application(const type_application& ) = default;
      const type ctor;
      const list<type> args;
    };

    
    struct lambda {

      struct typed {
        symbol ctor;
        symbol arg;
      };

      struct arg : variant<symbol, typed> {
        using arg::variant::variant;

        symbol name() const {
          if(is<symbol>()) return get<symbol>();
          return get<typed>().arg;
        }
      };
      
      lambda(const list<arg>& args, const expr& body)
        : args(args), body(body) { }

      lambda(const lambda&) = default;
      
      list<arg> args;
      expr body;
    };

    
    struct application {
      const expr func;
      const list<expr> args;
    };



    // let bindings
    struct definition {
      const symbol id;
      const expr value;
    };

    
    // monadic binding in a sequence
    struct binding {
      const symbol id;
      const expr value;
    };


    struct branch {
      const expr test, value;
    };


    // module definitions
    struct module {
      const type_constant ctor;
      const list<type_variable> args;
      
      struct row {
        const symbol name;
        const struct type type;
      };

      const list<row> rows;
    };


    // module export (rank2 -> rank1 boxing)
    struct export_ {
      symbol name;
      expr value;
    };
    

    
    // TODO type, etc
    struct toplevel : variant<expr, module> {
      using toplevel::variant::variant;
    };
    
    sexpr repr(const toplevel& self);
    
    toplevel check_toplevel(const sexpr& e);
    
    
  }
  
}



#endif
