#ifndef RIGID_MASS_HPP
#define RIGID_MASS_HPP

#include <core/metric.hpp>
#include <core/rigid.hpp>
#include <core/small_vector.hpp>


template<class U>
class rigid_mass : mass< rigid_transform<U> > {

  struct {
    small_vector<real> mass;
    small_vector<vec3> inertia;
  } storage;
  
public:


  rigid_mass(std::size_t n = 1) {
    resize(n);
  }

  slice<real>& mass = storage.mass;
  slice<vec3>& inertia = storage.inertia;
  
  std::size_t size() const { return storage.mass.size(); }

  void resize(std::size_t n) {
    storage.mass.resize(n);
    storage.inertia.resize(n);
  }


  using G = rigid_transform<U>;
  
  
  void tensor(triplet_iterator out, slice<const G> at) const {
    assert( size() == at.size() );
    
    for(unsigned i = 0, n = size(); i < n; ++i) {

      for(unsigned k = 0; k < 3; ++k) {
        const unsigned row = 6 * i + k;
        *out++ = triplet(row, row, storage.mass[i]);
      }

      for(unsigned k = 0; k < 3; ++k) {
        const unsigned row = 6 * i + 3 + k;
        *out++ = triplet(row, row, storage.inertia[i][k]);
      }

    }
  }
  

  // write spatial momentum based on body-fixed velocity
  virtual void momentum(slice< deriv<G> > out,
                        slice<const G> pos, slice<const deriv<G> > vel) const {
    assert(pos.size() == size());
    
	for(unsigned i = 0, n = size(); i < n; ++i) {
	  out[i].template head<3>() = mass[i] * vel[i].template head<3>();
      out[i].template tail<3>() = pos[i].orient * (inertia[i].array() * vel[i].template tail<3>().array());
	}
    
  }

  
  virtual void gravity(slice< deriv<G> > out,
                       slice<const G> pos, const vec3& g) const {
    assert(pos.size() == size());
    
    for(unsigned i = 0, n = size(); i < n; ++i) {
      out[i].template head<3>() = mass[i] * g;
    }

  }

  
};



#endif
