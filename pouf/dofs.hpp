#ifndef DOFS_HPP
#define DOFS_HPP

#include "vec.hpp"
#include "real.hpp"

#include "slice.hpp"

#include "../variant.hpp"

// dofs
template<class G> struct dofs;

struct dofs_base {
  virtual ~dofs_base() { }

  using cast_type = variant< 
    dofs<real>,
    dofs<vec1>,
    dofs<vec2>,
    dofs<vec3>,
    dofs<vec4>,
    dofs<vec6>>;
  
  virtual cast_type cast() = 0;

  virtual std::size_t size() const = 0;
  
};



template<class G>
struct dofs : public dofs_base,
              public std::enable_shared_from_this< dofs<G> > {

  dofs_base::cast_type cast() {
    return this->shared_from_this();
  }

  using coord_slice = slice<G>;
  using deriv_slice = slice< deriv<G> >;

  coord_slice& pos;
  deriv_slice& vel;
  deriv_slice& mom;
  
  dofs(coord_slice& pos,
	   deriv_slice& vel,
	   deriv_slice& mom)
	: pos(pos),
	  vel(vel),
	  mom(mom) {

  }
  
};


// TODO use small vectors + resizable instead ?
template<class G, std::size_t N = 1>
struct static_dofs : dofs<G> {

  struct {
	std::array<G, N> pos;
	std::array<deriv<G>, N> vel, mom;
  } storage;

  typename dofs<G>::coord_slice pos;
  typename dofs<G>::deriv_slice vel, mom;
  
  static_dofs()
	: dofs<G>(pos, vel, mom),
	pos(storage.pos.begin(), storage.pos.end()),
	vel(storage.vel.begin(), storage.vel.end()),
	mom(storage.mom.begin(), storage.mom.end()) {

  }
  
  std::size_t size() const { return N; }
  
  
};





#endif
