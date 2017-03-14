#ifndef UNIFORM_HPP
#define UNIFORM_HPP

#include <core/metric.hpp>

template<class Base>
struct uniform : Base {
  
  uniform(real value = 1.0) : value(value) { }
  
  real value;

  using G = typename Base::position_type;
  void tensor(triplet_iterator out, slice<const G> at) const {
    for(int i = 0, n = at.size() * traits<G>::dim; i < n; ++i) {
      *out++ = {i, i, value};
    }
  }
	
};



template<class G>
struct uniform_mass : uniform< mass<G> > {
  using uniform<mass<G>>::uniform;
  
  virtual void momentum(slice< deriv<G> > out, slice<const G> pos, slice<const deriv<G> > vel) const {
	for(unsigned i = 0, n = vel.size(); i < n; ++i) {
	  out[i] = this->value * vel[i];
	}
  }

  virtual void gravity(slice< deriv<G> > out, slice<const G> pos, const vec3& g) const;
  
};




template<class G>
struct uniform_stiffness : uniform< stiffness<G> > {
  using uniform<stiffness<G>>::uniform;

  // TODO compute logarithm for non-euclidean dofs?
  virtual void gradient(slice< deriv<G> > out, slice<const G> pos) const {
	for(unsigned i = 0, n = pos.size(); i < n; ++i) {
	  out[i] = this->value * pos[i];
	}
  }
};


template<class G>
struct uniform_damping : uniform< damping<G> > {
  using uniform<damping<G>>::uniform;
};


template<class G>
struct uniform_compliance : uniform< compliance<G> > {
  using uniform<compliance<G>>::uniform;
};




#endif
