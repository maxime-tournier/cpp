#include "uniform.hpp"
#include <core/func.hpp>


template<>
void uniform_mass<vec3>::gravity(slice< deriv<G> > out, 
								 slice<const G> pos, const vec3& g) const {
  for(unsigned i = 0, n = out.size(); i < n; ++i) {
	out[i] = g * this->value;
  }
}


template<>
void uniform_mass<rigid>::gravity(slice< deriv<G> > out, 
                                  slice<const G> pos, const vec3& g) const {
  for(unsigned i = 0, n = out.size(); i < n; ++i) {
	out[i].template head<3>() = g * this->value;
  }
}



template struct uniform_mass<vec3>;


