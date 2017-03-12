#ifndef POUF_SELECT_HPP
#define POUF_SELECT_HPP

#include "sparse.hpp"
#include "numbering.hpp"


// build primal/dual selection matrices
// TODO multiple strategies
struct select {

  using matrix_type = std::vector<triplet>;

  matrix_type& primal;
  matrix_type& dual;  

  std::size_t& primal_offset;
  std::size_t& dual_offset;  

  const numbering::chunks_type& chunks;
  
  void operator()(dofs_base* self, unsigned v) const {
	const chunk& c = chunks[v];
	for(unsigned i = c.start, n = c.start + c.size; i < n; ++i, ++primal_offset) {
	  primal.emplace_back(i, primal_offset, 1);
	}
  }

  void operator()(metric_base* self, unsigned v) const {
	self->cast.apply(*this, v);
  }

  

  template<class G>
  void operator()(metric<G>* self, unsigned v) const {

	if(self->cast.type() == metric<G>::compliance_type) {
	  const chunk& c = chunks[v];
	  for(unsigned i = c.start, n = c.start + c.size; i < n; ++i, ++dual_offset) {
		dual.emplace_back(i, dual_offset, 1);
	  }
	}
	
  }
  

  void operator()(func_base* self, unsigned v) const { };
  
};



#endif
