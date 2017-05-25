#ifndef TOOL_HPP
#define TOOL_HPP

#include "value.hpp"
#include "eval.hpp"

#include "../indices.hpp"

namespace lisp {
  
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*)(Args... args), indices<I...> indices) {
    static Ret(*ptr)(Args...) = f;
    
    return [](const value* first, const value* last) -> value {
      if((last - first) != sizeof...(Args)) {
        throw error("argc");
      }
 
      return ptr( first[I].template cast<Args>() ... );
    };
  } 
 
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*ptr)(Args... args)) {
    return wrap(f, ptr, typename tuple_indices<Args...>::type() );
  }
  
  template<class F>
  static builtin wrap(const F& f) {
    return wrap(f, +f);
  }


  // standard environment
  ref<context> std_env(int argc = 0, char** argv = nullptr);
  
}


#endif
