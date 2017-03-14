#ifndef NUMBERING_HPP
#define NUMBERING_HPP

#include "dispatch.hpp"

#include <core/graph.hpp>
#include <core/graph_data.hpp>

// deriv data numbering
struct chunk {
  std::size_t start, size;
};

struct numbering : dispatch<numbering> {

  using chunks_type = std::vector<chunk>;
  numbering(chunks_type& chunks,
            std::size_t& offset, 
            const graph_data& pos)
    : chunks(chunks),
      offset(offset),
      pos(pos) {

  }

  using dispatch::operator();
  
  chunks_type& chunks;
  std::size_t& offset;
  const graph_data& pos;


  template<class G>
  void push(unsigned v, std::size_t count) const {
	chunk& c = chunks[v];
	
	c.size = count * traits< deriv<G> >::dim;
	c.start = offset;
	
	offset += c.size;
  }

  
  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {
	if(self->cast.type() == metric<G>::compliance_type)  {
	  // compliant dofs get their slack dofs, with parent size
	  const unsigned p = *adjacent_vertices(v, g).first;
	  push<G>(v, pos.count(p));
	}
  }

  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, const graph& g) const {
	push<G>(v, pos.count(v));
  }


  template<class To, class ... From>
  void operator()(func<To (From...) >* self, unsigned v, const graph& g) const {
	push<To>(v, pos.count(v));
  }


  
};




#endif
