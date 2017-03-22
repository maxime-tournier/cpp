#include "uniform.hpp"
#include <core/func.hpp>


template<>
void uniform_mass<vec3>::force(slice< deriv<G> > out, 
                               slice<const G> pos, slice<const deriv<G> > vel,
                               const vec3& g) const {
  for(unsigned i = 0, n = out.size(); i < n; ++i) {
	out[i] = g * this->value;
  }
}


template<>
void uniform_mass<rigid>::force(slice< deriv<G> > out, 
                                slice<const G> pos, slice<const deriv<G> > vel,
                                const vec3& g) const {
  for(unsigned i = 0, n = out.size(); i < n; ++i) {
	out[i].template head<3>() = g * this->value;
  }
}



template struct uniform_mass<vec3>;


