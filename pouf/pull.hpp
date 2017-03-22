#ifndef POUF_PULL_HPP
#define POUF_PULL_HPP


// pull force/momentum and geometric stiffness
struct pull {
  
  vec& force;
  vec& momentum;
  
  rmat& jacobian;

  using matrix_type = std::vector<triplet>;
  matrix_type& gs;

  graph_data& work;
  
  const graph_data& pos;
  const numbering::chunks_type& chunks;
  const vec3& gravity;

  pull(vec& force,
	   vec& momentum,	   
	   rmat& jacobian,
	   matrix_type& gs,
	   graph_data& work,
	   const graph_data& pos,
	   const numbering::chunks_type& chunks,
	   const vec3& gravity)
	: force(force),
	  momentum(momentum),
	  jacobian(jacobian),
	  gs(gs),
	  work(work),
	  pos(pos),
	  chunks(chunks),
	  gravity(gravity) {

  }


  // dofs
  void operator()(dofs_base* self, unsigned v, const graph& g) const { }

  // metric
  void operator()(metric_base* self, unsigned v, const graph& g) const {
	self->cast.apply(*this, v, g);
  }

  template<class G>
  void operator()(metric<G>* self, unsigned v, const graph& g) const {
	self->cast.apply(*this, v, g);
  }


  template<class G>
  void operator()(mass<G>* self, unsigned v, const graph& g) const {

	const unsigned p = *adjacent_vertices(v, g).first;

	if( g[p].type() != graph::dofs_type ) {
	  throw std::runtime_error("mapped masses are not supported");
	}

	const dofs<G>* parent = static_cast<const dofs<G>*>(g[p].get<graph::dofs_type>());
	
	const chunk& c = chunks[p];

	slice<deriv<G>> tmp = work.allocate<deriv<G>>(v, pos.count(p));

	Eigen::Map<vec> view(tmp.template cast<real>().begin(), c.size);
	
	// momentum
	self->momentum(tmp, parent->pos, parent->vel);
	momentum.segment(c.start, c.size) += view;

    // force
	self->force(tmp, parent->pos, parent->vel, gravity);
	force.segment(c.start, c.size) += view;
	
  }


  template<class G>
  void operator()(stiffness<G>* self, unsigned v, const graph& g) const {

	// we need a copy due to alignement: typed data might expect
	// buffer to be aligned differently from data in force
	const unsigned p = *adjacent_vertices(v, g).first;
	  
	slice< deriv<G> > grad = work.allocate< deriv<G> >(v, pos.count(p));
	  
	self->gradient( grad, pos.get<G>(p));
	  
	const chunk& c = chunks[p];
	
	// add gradient
	force.segment(c.start, c.size) -= Eigen::Map<vec>((real*) grad.begin(), c.size);
	
	// std::clog << "stiffness force: " << gradient.segment(c.start, c.size).transpose() << std::endl;
  }


  void operator()(func_base* self, unsigned v, const graph& g) const {
	self->cast.apply(*this, v, g);
  }

  
  template<class To, class ... From>
  void operator()(func<To (From...)>* self, unsigned v, const graph& g) const {

	const chunk& c = chunks[v];

	// pull data by mapping
	force.noalias() += jacobian.middleRows(c.start, c.size).transpose() * force.segment(c.start, c.size);

	// std::clog << "func force: " << gradient.segment(c.start, c.size).transpose() << std::endl;
	
	// TODO geometric stiffness
  }


  template<class T>
  void operator()(T*, unsigned v, const graph& g) const { }
  
  
};


#endif
