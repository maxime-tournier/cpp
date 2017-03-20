#ifndef INTEGRATE_HPP
#define INTEGRATE_HPP

#include <core/vec.hpp>
#include <core/graph_data.hpp>

#include "numbering.hpp"

struct integrate {

  const numbering::chunks_type& chunks;
  const vec& velocities;
  const real dt;


  template<class T>
  void operator()(T* self, unsigned v, graph& g) const {

  }
  
  void operator()(dofs_base* self, unsigned v, graph& g) const {
	self->cast.apply(*this, v, g);
  }

  void operator()(metric_base* self, unsigned v, graph& g) const {
    self->cast.apply(*this, v, g);
  }
  

  template<class G>
  void operator()(metric<G>* self, unsigned v, graph& g) const {
    self->cast.apply(*this, v, g);
  }


  template<class G>
  void operator()(mass<G>* self, unsigned v, graph& g) const {

    const unsigned p = g.parent(v);

    assert(g[p].type() == graph::dofs_type);
    
    auto d = g[p].get<graph::dofs_type>();
    auto dg = static_cast<dofs<G>*>(d);

    // TODO save spatial momentum
    
    self->momentum(dg->mom, dg->pos, dg->vel);
  }
  

  void operator()(func_base* self, unsigned v, graph& g) const {

  }
  
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, graph& g) const {

	const chunk& c = chunks[v];
	auto segment = velocities.segment(c.start, c.size);
	
	for(unsigned i = 0, n = self->size(); i < n; ++i) {
	  for(unsigned j = 0; j < traits<deriv<G>>::dim; ++j) {
		traits<deriv<G>>::coord(j, self->vel[i]) = segment(i * n + j);
	  }

	  // std::clog << "integration: " << self->vel[i].transpose() << std::endl;
	  
	  self->pos[i] = prod(self->pos[i], traits<G>::exp(dt * self->vel[i]));
	}
	
  }
  
  
};



#endif
