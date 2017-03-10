#ifndef NUMBERING_HPP
#define NUMBERING_HPP

#include "dispatch.hpp"
#include "graph_data.hpp"

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
  void operator()(metric<G>* self, unsigned v) const {
    // TODO don't dispatch
  }
  
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v) const {
    chunk& c = chunks[v];
	
    c.size = pos.count(v) * traits< deriv<G> >::dim;
    c.start = offset;
    
    offset += c.size;
  }


  template<class To, class ... From>
  void operator()(func<To (From...) >* self, unsigned v) const {
    chunk& c = chunks[v];
	
    c.size = pos.count(v) * traits< deriv<To> >::dim;
    c.start = offset;
    
    offset += c.size;
  }  
  
};




#endif
