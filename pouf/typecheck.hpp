#ifndef TYPECHECK_HPP
#define TYPECHECK_HPP


#include <stdexcept>
#include <cxxabi.h>

#include "graph.hpp"

#include "metric.hpp"
#include "dofs.hpp"
#include "func.hpp"


static std::string demangle(const char* name) {

  int status;

  // enable c++11 by passing the flag -std=c++11 to g++
  std::unique_ptr<char, void(*)(void*)> res = {
    abi::__cxa_demangle(name, NULL, NULL, &status),
    std::free
  };

  return (status==0) ? res.get() : name ;
}

struct typecheck {

  void operator()(dofs_base* self, unsigned v, const graph& g) const {
    if( out_degree(v, g) > 0 ) {
      throw std::runtime_error("dofs must be independent (zero out-edge)");
    }
  }

  void operator()(metric_base* self, unsigned v, const graph& g) const {

    if( out_degree(v, g) != 1 ) {
      throw std::runtime_error("metric must be dependent (single out-edge)");
    }

    self->cast().apply(*this, v, g);
  }


  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {

    auto p = *adjacent_vertices(v, g).first;
	
    g[p].apply(expected_check<G>());
  }

  // dispatch
  void operator()(func_base* self, unsigned v, const graph& g) const {
    self->cast().apply(*this, v, g);
  }



  struct type_error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  struct edge_error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  
  template<class Expected>
  struct expected_check {

    template<class T>
    void operator()(T* self) const {
      throw std::logic_error("should not happen");
    }
	
    // dispatch
    void operator()(dofs_base* self) const {
      self->cast().apply(*this);
    }

    void operator()(func_base* self) const {
      self->cast().apply(*this);
    }

    // ok cases
    void operator()(dofs<Expected>* ) const { };

    template<class ... From>
    void operator()(func<Expected (From...) >* ) const { };	


    // error cases
    template<class T>
    void operator()(dofs<T>* ) const {
      throw std::runtime_error("type error: "+ demangle( typeid(Expected (T) ).name() ));
    }


    template<class T, class ... From>
    void operator()(func<T (From...) >* ) const {
      throw std::runtime_error("type error: " + demangle( typeid(Expected (T) ).name() ));
    }
	
  };
  
  template<class Expected, class Iterator, class G>
  static void check_type(Iterator first, Iterator last, const G& g) {
    if(first == last) {
      throw std::runtime_error("expected more out-edges");
    }

    const unsigned v = *first;
	
    g[v].apply(expected_check<Expected>());
  }


  // typecheck from types
  template<class To, class ... From>
  void operator()(func<To (From...)>* self, unsigned v, const graph& g) const {

    auto out = adjacent_vertices(v, g);
    try {
      const int expand[] = { (check_type<From>(out.first++, out.second, g), 0)...};

      // TODO this could be ok though
      if(out.first != out.second) {
        throw std::runtime_error("expected less out-edges");
      }
    } catch( std::runtime_error& e ){
      throw std::runtime_error(e.what() 
                               + std::string(" for vertex: ") 
                               + std::to_string(v) );
    }
	
  }
  
};




#endif
