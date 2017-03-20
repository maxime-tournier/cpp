#ifndef INIT_HPP
#define INIT_HPP

#include <core/vec.hpp>
#include <core/graph_data.hpp>


struct init {

  graph_data& init_pos;
  graph_data& init_vel;  
  
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

    // compute momentum
    const unsigned p = *adjacent_vertices(v, g).first;

    // TODO typesafe
    assert( g[p].type() == graph::dofs_type );
    dofs_base* d = g[p].get< graph::dofs_type >();

    auto dg = static_cast<dofs<G>*>(d);
    
    self->momentum( dg->mom, dg->pos, dg->vel );
    
    // std::clog << "init momentum: " << dg->mom[0] << std::endl;
  }
  
  void operator()(func_base* self, unsigned v, graph& g) const {

  }
  
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v, graph& g) const {
	slice<G> pos = init_pos.allocate<G>(v, self->size());
	slice<deriv<G>> vel = init_vel.allocate< deriv<G> >(v, self->size());	

    pos << self->pos;
    vel << self->vel;
  }
	
};



#endif
