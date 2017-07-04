#ifndef SLIP_AST_HPP
#define SLIP_AST_HPP

#include "list.hpp"
#include "symbol.hpp"
#include "sexpr.hpp"

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
                           ref<sequence>,
                           ref<condition> > {
      using expr::variant::variant;

      using list = slip::list<expr>;

    };
                           
                           
    
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


    struct definition {
      definition(const symbol& name, const expr& value)
        : name(name),
          value(value) { }
        
      const symbol name;
      const expr value;
    };

    
    struct sequence {

      struct binding {
        binding(const symbol& name, const expr& value)
          : name(name),
            value(value) { } 
         
        const symbol name;
        const expr value;
      };

      struct item : variant< expr, binding > {
        using item::variant::variant;
      };

      list<item> items;
      
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
    
    struct toplevel : variant<sequence::item> {
      using toplevel::variant::variant;
    };
    

    toplevel check(const sexpr& e);

    
  }
  
}



#endif
