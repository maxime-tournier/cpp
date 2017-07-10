#ifndef SLIP_AST_HPP
#define SLIP_AST_HPP

#include "list.hpp"
#include "symbol.hpp"
#include "sexpr.hpp"


// TODO repr ?

namespace slip {

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

    template<class T>
    struct literal {
      const T value;
    };


    struct variable {
      const symbol name;
    };

    

    struct expr : variant< literal<unit>,
                           literal<boolean>,
                           literal<integer>,
                           literal<real>,
                           literal<string>,
                           variable,
                           ref<lambda>,
                           ref<application>,
                           ref<definition>,
                           ref<binding>,                           
                           ref<sequence>,
                           ref<condition> > {
      using expr::variant::variant;

      using list = slip::list<expr>;

    };

    sexpr repr(const expr& self);
    std::ostream& operator<<(std::ostream& out, const expr& self);
    
    
    struct lambda {
      lambda(const list<symbol>& args, const expr& body)
        : args(args), body(body) { }
      
      const list<symbol> args;
      const expr body;
    };

    
    struct application {
      application(const expr& func, const list<expr>& args)
        : func(func), args(args) { }
        
      const expr func;
      const list<expr> args;
    };


    // let bindings
    struct definition {
      definition(const symbol& name, const expr& value)
        : name(name),
          value(value) { }
        
      const symbol name;
      const expr value;
    };

    
    // monadic binding in a sequence
    struct binding {
      binding(const symbol& name, const expr& value)
        : name(name),
          value(value) { } 
      
      const symbol name;
      const expr value;
    };

    
    struct sequence {
      list<expr> items;
    };

    
    struct condition {
      
      struct branch {
        branch(const expr& test, const expr& value)
          : test(test), value(value) { }

        expr test, value;
      };

      condition(const list<branch>& branches)
        : branches(branches) { }
      
      list<branch> branches;
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
