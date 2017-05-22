#include <iostream>
#include "parse.hpp"

#include <sstream>
#include <string>

#include <set>
#include <memory>
#include <vector>
#include <deque>
#include <cassert>
#include <fstream>

#include <unordered_map>
#include <map>

#include <readline/readline.h>
#include <readline/history.h>

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


  static value list_ctor(const value* first, const value* last) {
    list res;
    list* curr = &res;
    
    for(const value* it = first; it != last; ++it) {
      *curr = make_ref<cell>(*it, nil);
      curr = &(*curr)->tail;
    }
                                                 
    return res;
  }

  struct eq_visitor {

    template<class T>
    void operator()(const T& self, const value& other, bool& result) const {
      result = self == other.template get<T>();
    }
    
  };
  
}

template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);

	std::stringstream ss(line);

	if( f(ss) ) {
      // success
	} else if(!std::cin.eof()) {
	  std::cerr << "parse error" << std::endl;
	}
    
  }
  
};






int main(int argc, char** argv) {
  using namespace lisp;
  ref<context> ctx = make_ref<context>();

  (*ctx)
    ("add", +[](const value* first, const value* last) -> value {
      integer res = 0;
      while(first != last) {
        res += (first++)->cast<integer>();
      };
      return res;
    })
    ("sub", wrap([](integer x, integer y) -> integer { return x - y; }))
    ("mul", wrap([](integer x, integer y) -> integer { return x * y; }))
    ("div", wrap([](integer x, integer y) -> integer { return x / y; }))
    ("mod", wrap([](integer x, integer y) -> integer { return x % y; }))        
    
    ("list", list_ctor)
    ("print", +[](const value* first, const value* last) -> value {
      bool start = true;
      for(const value* it = first; it != last; ++it) {
        if(start) start = false;
        else std::cout << ' ';
        std::cout << *it;
      }
      std::cout << std::endl;
      return nil;
    })
    ("eq", +[](const value* first, const value* last) -> value {
      if(last - first < 2) throw error("argc");
      if(first[0].type() != first[1].type()) return nil;
      bool res;
      first[0].apply(eq_visitor(), first[1], res);
      if(!res) return nil;

      static symbol t("t");
      return t;
    })
    ;
    
   
  const auto parser = *lisp::parser() >> [&](std::deque<value>&& exprs) {
    try{

      for(const value& e : exprs) {
        const value val = eval(ctx, e);
        if(val) {
          std::cout << val << std::endl; 
        }
      }
      
    } catch( error& e ) {
      std::cerr << "error: " << e.what() << std::endl;
    }
    
    return parse::pure(exprs);
  };

  if( argc > 1 ) {
    std::ifstream file(argv[1]);
    
    if(!parser(file)) {
      std::cerr << "parse error" << std::endl;
      return 1;
    }
    
  } else {
  
    read_loop([parser](std::istream& in) {
        return bool( parser(in) );
      });
  }
 
  return 0;
}
 
 
