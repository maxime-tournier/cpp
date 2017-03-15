#ifndef RESET_HPP
#define RESET_HPP

#include <core/vec.hpp>
#include <core/graph_data.hpp>

struct reset {

  const graph_data& init_pos;
  const graph_data& init_vel;  
  
  void operator()(dofs_base* self, unsigned v) const {
	self->cast.apply(*this, v);
  }

  void operator()(metric_base* self, unsigned v) const {

  }


  void operator()(func_base* self, unsigned v) const {

  }
  
  
  template<class G>
  void operator()(dofs<G>* self, unsigned v) const {
	assert(self->size() == init_pos.count(v));
	assert(self->size() == init_vel.count(v));

    std::clog << "init pos: " << init_pos.get<G>(v)[0] << std::endl;
	self->pos = init_pos.get<G>(v);
	self->vel = init_vel.get< deriv<G> >(v);	
  }
	
};



#endif
