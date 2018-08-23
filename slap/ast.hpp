#ifndef AST_HPP
#define AST_HPP

#include "ref.hpp"
#include "variant.hpp"
#include "list.hpp"
#include "symbol.hpp"
#include "base.hpp"

struct sexpr;

namespace ast {

  // expressions
  template<class T>
  struct lit {
    const T value;
  };


  template<class T>
  static lit<T> make_lit(T value) { return {value}; }

  struct expr;

  static inline ref<expr> make_expr(const expr& e) { return make_ref<expr>(e); }
  static inline ref<expr> make_expr(expr&& e) { return make_ref<expr>(std::move(e)); }  
  
  struct app {
    app(const expr& func, const list<expr>& args)
      : func(make_ref<expr>(func)),
        args(args) { }
    
    const ref<expr> func;
    const list<expr> args;
  };


  struct abs {
    struct typed {
      const symbol type;
      const symbol name;
    };

    struct arg : variant<symbol, typed> {
      using arg::variant::variant;
      symbol name() const;
    };
    
    const list<arg> args;
    const ref<expr> body;

    abs(const list<arg>& args, const expr& body)
      : args(args), body(make_expr(body)) { }
  };

  struct def;

  struct bind;
  
  struct let {
    let(const list<bind>& defs, const expr& body)
      : defs(defs), body(make_expr(body)) { }
    
    const list<bind> defs;
    const ref<expr> body;
  };

  struct var {
    const symbol name;
  };


  // conditionals
  struct cond {
    cond(const expr& test,
         const expr& conseq,
         const expr& alt)
      : test(make_expr(test)),
        conseq(make_expr(conseq)),
        alt(make_expr(alt)) { }
    
    const ref<expr> test;
    const ref<expr> conseq;
    const ref<expr> alt;
  };


  // records
  struct rec {
    struct attr;

    const list<attr> attrs;
  };


  // attribute selection
  struct sel {
    const symbol name;
  };




  // module packing
  struct make {
    const symbol type;
    const list<rec::attr> attrs;
  };

  // definition (modifies current environment)
  struct def {
    const symbol name;
    const ref<expr> value;
    def(symbol name, const expr& value)
      : name(name), value(make_expr(value)) { }
  };

  // sequencing
  struct io;
  
  struct seq {
    const list<io> items;
  };

  struct expr : variant<lit<boolean>,
                        lit<integer>,
                        lit<real>,
                        var, abs, app, let,
                        cond,
                        rec, sel,
                        make,
                        def, seq> {
    using expr::variant::variant;

    friend std::ostream& operator<<(std::ostream& out, const expr& self);

    static expr check(const sexpr& e);
  };


  struct rec::attr {
    using list = list<attr>;
    const symbol name;
    const expr value;
  };

  // monadic binding in a sequence
  struct bind {
    const symbol name;
    const expr value;
  };
  
  // stateful computations
  struct io : variant<bind, expr>{
    using io::variant::variant;

    static io check(const sexpr& e);
    friend std::ostream& operator<<(std::ostream& out, const io& self);    
  };


  struct toplevel : variant<expr> {
    using toplevel::variant::variant;
    
    // TODO types, modules etc
    friend std::ostream& operator<<(std::ostream& out, const toplevel& self);

    static toplevel check(const sexpr& e);
  };

  
  struct error : std::runtime_error {
    using runtime_error::runtime_error;
  };

  
}


#endif