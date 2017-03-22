#ifndef INIT_HPP
#define INIT_HPP

#include <core/vec.hpp>
#include <core/graph_data.hpp>


struct init {

  graph_data& init_pos;
  graph_data& init_vel;  


  void operator()(metric_base* self, unsigned v, graph& g) const { }
  void operator()(func_base* self, unsigned v, graph& g) const { }
  
  void operator()(dofs_base* self, unsigned v, graph& g) const {
	self->cast.apply(*this, v, g);
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
