#ifndef EVAL_HPP
#define EVAL_HPP

#include <map>
#include <vector>

#include "error.hpp"
#include "sexpr.hpp"
#include "context.hpp"

namespace lisp {

  struct unbound_variable : error {
    unbound_variable(const symbol& s);
    
  };


  struct value;
  
  struct environment;
  struct lambda;
  
  // TODO pass ctx ?
  using builtin = value (*)(const value* first, const value* last);
  

  struct value : variant<unit, boolean, integer, real, symbol, ref<string>, list<value>,
                         builtin, ref<lambda> > {
    
    using list = lisp::list<value>;
    
    using value::variant::variant;

    value(const sexpr& other) : value::variant( reinterpret_cast<const variant&> (other)) { }
    value(sexpr&& other) : value::variant( reinterpret_cast<variant&&> (other)) { }    
    
  };


  std::ostream& operator<<(std::ostream& out, const value& self);


  // lambda
  struct lambda {

    ref<environment> ctx;
    using args_type = std::vector<symbol>;
    args_type args;
    sexpr body;


    lambda(const ref<environment>& ctx,
           args_type&& args,
           const sexpr& body);
    
    
  };

  
  
  struct environment : lisp::context<environment, value> {

    using context::context;
    
    using macros_type = std::map< symbol, value >;
    macros_type macros;
    
    environment& operator()(const symbol& name, const value& expr);
  };


  template<class Iterator>
  static ref<environment> extend(const ref<environment>& parent, Iterator first, Iterator last) {
    ref<environment> res = make_ref<environment>(parent);
    res->locals.insert(first, last);
    
    return res;
  }


  value eval(const ref<environment>& ctx, const sexpr& expr);
  value apply(const value& app, const value* first, const value* last);
  
}

#endif
