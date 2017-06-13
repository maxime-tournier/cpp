#ifndef EVAL_HPP
#define EVAL_HPP

#include <map>
#include <vector>

#include "error.hpp"
#include "sexpr.hpp"

namespace lisp {

  struct unbound_variable : error {
    unbound_variable(const symbol& s);
    
  };


  struct value;
  
  struct context;
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

    ref<context> ctx;
    using args_type = std::vector<symbol>;
    args_type args;
    sexpr body;


    lambda(const ref<context>& ctx,
           args_type&& args,
           const sexpr& body);
    
    
  };

  
  
  struct environment : lisp::context<value> {
    
    ref<context> parent;
    context(const ref<context>& parent = {}) 
      : parent(parent) { }
    
    using locals_type = std::map< symbol, value >;
    locals_type locals;

    using macros_type = std::map< symbol, value >;
    macros_type macros;
    
    value& find(const symbol& name);

    context& operator()(const symbol& name, const value& expr);
    
  };


  template<class Iterator>
  static ref<context> extend(const ref<context>& parent, Iterator first, Iterator last) {
    ref<context> res = make_ref<context>(parent);
    res->locals.insert(first, last);
    
    return res;
  }


  value eval(const ref<context>& ctx, const sexpr& expr);
  value apply(const value& app, const value* first, const value* last);
  
}

#endif
