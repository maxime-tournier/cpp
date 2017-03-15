#ifndef INIT_HPP
#define INIT_HPP

#include <core/vec.hpp>
#include <core/graph_data.hpp>


struct init {

  graph_data& init_pos;
  graph_data& init_vel;  
  
  void operator()(dofs_base* self, unsigned v) const {
	self->cast.apply(*this, v);
  }

  void operator()(metric_base* self, unsigned v) const {

  }


  void operator()(func_base* self, unsigned v) const {

  }
  
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v) const {

	slice<G> pos = init_pos.allocate<G>(v, self->size());
	slice<deriv<G>> vel = init_vel.allocate< deriv<G> >(v, self->size());	

	std::clog << "init pos: " << self->pos[0] << std::endl;
	pos = self->pos;
	vel = self->vel;

    std::clog << "init pos: " << pos[0] << std::endl;
  }
	
};



#endif
