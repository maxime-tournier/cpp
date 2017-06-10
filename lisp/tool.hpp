#ifndef LISP_TOOL_HPP
#define LISP_TOOL_HPP

#include "eval.hpp"

#include "../indices.hpp"
#include <iostream>

namespace lisp {
  
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*)(Args... args), indices<I...> indices) {
    static Ret(*ptr)(Args...) = f;
    
    return +[](const value* first, const value* last) -> value {
      if((last - first) != sizeof...(Args)) {
        throw error("argc");
      }

      try{
        return ptr( first[I].template cast<Args>() ... );
      } catch( value::bad_cast& e) {
        throw type_error("bad type for builtin");
      }
      
      return ptr( first[I].template get<Args>() ... );      
    };
  } 
 
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*ptr)(Args... args)) {
    return wrap(f, ptr, type_indices<Args...>() );
  }
  
  template<class F>
  static builtin wrap(const F& f) {
    return wrap(f, +f);
  }


  // standard environment
  ref<context> std_env(int argc = 0, char** argv = nullptr);
  
}


#endif
