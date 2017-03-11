#ifndef FETCH_HPP
#define FETCH_HPP

#include "dispatch.hpp"
#include "graph_data.hpp"
#include "types.hpp"
#include "numbering.hpp"

// fetch jacobians w/ masking (bottom-up)
struct fetch : dispatch<fetch> {

  using matrix_type = std::vector<triplet>;
  matrix_type& jacobian;
  matrix_type& diagonal;
  
  graph_data& mask;
  
  using elements_type = std::vector< matrix_type >;
  
  // internal
  elements_type& elements;

  const numbering::chunks_type& chunks;
  const graph_data& pos;
  
  fetch(matrix_type& jacobian,
        matrix_type& diagonal,
        graph_data& mask,
        elements_type& elements,
        const numbering::chunks_type& chunks,	
        const graph_data& pos)
    : jacobian(jacobian),
      diagonal(diagonal),
      mask(mask),
      elements(elements),
      chunks(chunks),
      pos(pos) {
    
  }

  real dt = 1.0;
  
  using dispatch::operator();

  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {

    // remember current size
    const std::size_t start = diagonal.size();

    // obtain tensor triplets
	auto it = std::back_inserter(diagonal);
    self->tensor(it, pos.get<G>(v));
	
    const std::size_t end = diagonal.size();

    real factor = 0;

    switch(self->kind) {
    case metric_kind::mass: factor = 1; break;
    case metric_kind::damping: factor = dt; break;
    case metric_kind::stiffness: factor = dt * dt; break;
    case metric_kind::compliance: factor = -1.0 / (dt * dt); break;	        
    };
    
    const unsigned p = *adjacent_vertices(v, g).first;
	
    const chunk& parent = chunks[p];
	
    // shift/scale inserted data
    for(unsigned i = start; i < end; ++i) {
      auto& it = diagonal[i];
      it = {it.row() + int(parent.start),
            it.col() + int(parent.start),
            factor * it.value()};
    }
	
	
	// compliance slack variable stiffness
	if(self->kind == metric_kind::compliance) {
	  const chunk& curr = chunks[v];

	  for(unsigned i = 0, n = curr.size; i < n; ++i) {
		
		// TODO only lower diagonal should be needed
		*it++ = {parent.start + i, curr.start + i, 1};
		*it++ = {curr.start + i, parent.start + i, 1};
	  }
	}
	
  }

  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
    const chunk& c = chunks[v];

    for(unsigned i = c.start, n = c.start + c.size; i < n; ++i) {
      jacobian.emplace_back(i, i, 1.0);
    }
  }
    
  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
    operator()(self, v, g, indices_for<From...>() );
  }


  template<class To, class ... From, std::size_t ... I>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g,
                  indices<I...> idx) const {

    auto parents = adjacent_vertices(v, g).first;

    // get dofs count
    const std::size_t count = pos.count(v);
    const std::size_t parent_count[] = { pos.count(parents[I])... };
      
    const std::size_t rows = count * traits< deriv<To> >::dim;
    const std::size_t cols[] = { parent_count[I] * traits< deriv<From> >::dim... };    
    
    // no child: allocate mask
    if( in_degree(v, g) == 0 ) {
      mask.allocate<char>(v, count);
    }

    const std::size_t n = sizeof...(From);
    
    if( elements.size() < n) elements.resize(n);
    
    for(unsigned p = 0; p < n; ++p) {
      // alloc parent masks
      const unsigned vp = parents[p];
      mask.allocate<char>(vp, parent_count[p]);

      // TODO make sure this does not dealloc
      elements[p].clear();
    }

    // fetch jacobian data
    elements.clear();
    self->jacobian(std::back_inserter(elements[I])...,
                   pos.get<const From>(parents[I])...);
    
    // TODO set parent masks
    const chunk& curr_chunk = chunks[v];
    
    for(unsigned p = 0; p < n; ++p) {
      const unsigned vp = parents[p];
      const chunk& parent_chunk = chunks[vp];
	
      // shift elements
      for(triplet& it : elements[p]) {
        // TODO filter by mask
        jacobian.emplace_back(it.row() + curr_chunk.start,
                              it.col() + parent_chunk.start,
                              it.value());
      }
    }
    
  }
  
};



#endif
