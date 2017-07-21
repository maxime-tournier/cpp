#ifndef LISP_TOOL_HPP
#define LISP_TOOL_HPP

#include "eval.hpp"

#include "../indices.hpp"
#include <iostream>

namespace slip {
  
  template<class Value, class F, class Ret, class ... Args, std::size_t ... I>
  static Value wrap(const F& f, Ret (*)(Args... args), indices<I...> indices, bool check = true) {
    static Ret(*ptr)(Args...) = f;

    if( check ) {
      return +[](const Value* first, const Value* last) -> Value {
        argument_error::check(last - first, sizeof...(Args));
        
        try{
          return ptr( std::move(first[I].template cast<Args>()) ... );
        } catch( typename Value::bad_cast& e) {
          throw type_error("bad type for builtin arg");
        }

      };
    } else {
      return +[](const Value* first, const Value* last) -> Value {
        return ptr( std::move(first[I].template get<Args>()) ... );
      };
    }
  } 
 
  template<class Value, class F, class Ret, class ... Args>
  static Value wrap(const F& f, Ret (*ptr)(Args... args), bool check = true) {
    return wrap<Value>(f, ptr, type_indices<Args...>(), check );
  }
  
  template<class Value, class F>
  static Value wrap(const F& f, bool check = true) {
    return wrap<Value>(f, +f, check);
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
