#ifndef PUSH_HPP
#define PUSH_HPP


#include "dispatch.hpp"

#include <core/graph_data.hpp>
#include <core/indices.hpp>

// push positions (top-down)
struct push : dispatch<push> {

  graph_data& pos;
  
  push(graph_data& pos) : pos(pos) { }
  
  
  using dispatch::operator();

  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {

  }
  
  
  // push dofs data to the stack  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
    const std::size_t count = self->size();

    // allocate
    slice<G> data = pos.allocate<G>(v, count);
    
    // TODO static_assert G is pod ?
    
    // copy
	std::copy(self->pos.begin(), self->pos.end(), data.begin());
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
    operator()(self, v, g, indices_for<From...>() );
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g,
                  indices<I...> idx) const {

    // note: we need random access parent iterator for this
    auto parents = adjacent_vertices(v, g).first;
    
    // result size
    const std::size_t count =
      self->size( pos.get<const From>(parents[I])... );
    
    // allocate result
    slice<To> to = pos.allocate<To>(v, count);
    
    self->apply(to, pos.get<const From>(parents[I])...);

    std::clog << "mapped position: " << to[0] << std::endl;
  }
  
  
};



#endif
