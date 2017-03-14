#include "uniform.hpp"
#include "func.hpp"


template<>
struct uniform_mass<vec3>::helper {

  static vec3 gravity(const uniform_mass<vec3>* self, const vec& , const vec& g) {
	return g * self->value;
  }
};

template<class G>
void uniform_mass<G>::gravity(slice< deriv<G> > out, slice<const G> pos, const vec3& g) const {
  for(unsigned i = 0, n = out.size(); i < n; ++i) {
	out[i] += helper::gravity(this, pos[i], g);
  }
}


template class uniform_mass<vec3>;


