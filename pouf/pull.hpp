#ifndef POUF_PULL_HPP
#define POUF_PULL_HPP


// pull gradient/momentum and geometric stiffness
struct pull {
  
  vec& gradient;
  vec& momentum;
  
  rmat& jacobian;

  using matrix_type = std::vector<triplet>;
  matrix_type& gs;

  graph_data& work;
  
  const graph_data& pos;
  const numbering::chunks_type& chunks;


  pull(vec& gradient,
	   vec& momentum,	   
	   rmat& jacobian,
	   matrix_type& gs,
	   graph_data& work,
	   const graph_data& pos,
	   const numbering::chunks_type& chunks)
	: gradient(gradient),
	  momentum(momentum),
	  jacobian(jacobian),
	  gs(gs),
	  work(work),
	  pos(pos),
	  chunks(chunks) {

  }

  
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

	// TODO 
	const dofs<G>* parent = static_cast<const dofs<G>*>(g[p].get<graph::dofs_type>());
	
	const chunk& c = chunks[p];

	slice<deriv<G>> mu = work.allocate<deriv<G>>(v, pos.count(p));
	
	self->momentum(mu, parent->pos, parent->vel);
	momentum.segment(c.start, c.size) += Eigen::Map<vec>((real*) mu.begin(), c.size);

	// std::clog << "mass momentum: " << momentum.segment(c.start, c.size).transpose() << std::endl;
  }


  template<class G>
  void operator()(stiffness<G>* self, unsigned v, const graph& g) const {

	// we need a copy due to alignement: typed data might expect
	// buffer to be aligned differently from data in gradient
	const unsigned p = *adjacent_vertices(v, g).first;
	  
	slice< deriv<G> > grad = work.allocate< deriv<G> >(v, pos.count(p));
	  
	self->gradient( grad, pos.get<G>(p));
	  
	const chunk& c = chunks[p];
	
	// add gradient
	gradient.segment(c.start, c.size) += Eigen::Map<vec>((real*) grad.begin(), c.size);
	
	// std::clog << "stiffness gradient: " << gradient.segment(c.start, c.size).transpose() << std::endl;
  }


  void operator()(func_base* self, unsigned v, const graph& g) const {
	self->cast.apply(*this, v, g);
  }
  
  template<class To, class ... From>
  void operator()(func<To (From...)>* self, unsigned v, const graph& g) const {

	const chunk& c = chunks[v];

	// pull data by mapping
	gradient.noalias() += jacobian.middleRows(c.start, c.size).transpose() * gradient.segment(c.start, c.size);

	// std::clog << "func gradient: " << gradient.segment(c.start, c.size).transpose() << std::endl;
	
	// TODO geometric stiffness
  }

  template<class T>
  void operator()(T*, unsigned v, const graph& g) const { }
  
  
};


#endif
