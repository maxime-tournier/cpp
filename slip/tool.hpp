#ifndef LISP_TOOL_HPP
#define LISP_TOOL_HPP

#include "eval.hpp"

#include "../indices.hpp"
#include <iostream>

namespace slip {
  
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*)(Args... args), indices<I...> indices) {
    static Ret(*ptr)(Args...) = f;
    
    return +[](const value* first, const value* last) -> value {
      argument_error::check(last - first, sizeof...(Args));
      
      try{
        return ptr( std::move(first[I].template cast<Args>()) ... );
      } catch( value::bad_cast& e) {
        throw type_error("bad type for builtin");
      }
      
      // return ptr( first[I].template get<Args>() ... );      
    };
  } 
 
  template<class F, class Ret, class ... Args>
  static builtin wrap(const F& f, Ret (*ptr)(Args... args)) {
    return wrap(f, ptr, type_indices<Args...>() );
  }
  
  template<class F>
  static builtin wrap(const F& f) {
    return wrap(f, +f);
  }

  template<std::size_t ...I, class Value>
  static std::array<Value, sizeof...(I)> unpack_args(const Value* first, 
                                                     const Value* last,
                                                     indices<I...> ) {
    argument_error::check(last - first, sizeof...(I));
    return {{std::move(first[I])...}};
  }

  
  template<std::size_t N, class Value>
  static std::array<Value, N> unpack_args(const Value* first, const Value* last) {
    return unpack_args(first, last, range_indices<N>());
  }

  

  // standard environment
  ref<environment> std_env(int argc = 0, char** argv = nullptr);
  
}


#endif
