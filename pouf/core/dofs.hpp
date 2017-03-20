#ifndef CORE_DOFS_HPP
#define CORE_DOFS_HPP

#include "vec.hpp"
#include "real.hpp"
#include "rigid.hpp"

#include "slice.hpp"
#include "small_vector.hpp"

#include "api.hpp"


// dofs
template<class G> struct dofs;

struct dofs_base {
  virtual ~dofs_base() { }

  using cast_type = api< 
    dofs<real>,
    dofs<vec2>,
    dofs<vec3>,
    dofs<vec6>,
    dofs<rigid>
	>;
  
  const cast_type cast;

  virtual std::size_t size() const = 0;

protected:
  template<class Derived>
  dofs_base(Derived* self)
	: cast(self, &dofs_base::cast) {
	assert(self == this);
  }
  
};



template<class G>
struct dofs : public dofs_base {

  using coord_slice = slice<G>;
  using deriv_slice = slice< deriv<G> >;

  coord_slice& pos;
  deriv_slice& vel;             // body-fixed
  deriv_slice& mom;             // spatial
  
  dofs(coord_slice& pos,
	   deriv_slice& vel,
	   deriv_slice& mom)
	: dofs_base(this),
	  pos(pos),
	  vel(vel),
	  mom(mom) {

  }
  
};


// TODO move outside core

// TODO use small vectors + resizable instead ?
template<class G, std::size_t N = 1>
struct static_dofs : dofs<G> {

  struct {
	std::array<G, N> pos;
	std::array<deriv<G>, N> vel, mom;
  } storage;

  slice<G> pos;
  slice<deriv<G> > vel, mom;
  
  static_dofs()
	: dofs<G>(pos, vel, mom),
    pos(&storage.pos, &storage.pos + 1),
    vel(&storage.vel, &storage.vel + 1),
    mom(&storage.mom, &storage.mom + 1)    
  {
    
  }
  
  std::size_t size() const { return N; }
};

template<class G>
struct dynamic_dofs : dofs<G> {
  
  small_vector<G> pos;
  small_vector<deriv<G>> vel, mom;
  
  std::size_t size() const { return dofs<G>::pos.size(); }

  void resize(std::size_t n) {
    pos.resize(n);
    vel.resize(n);
    mom.resize(n);
  }

  
  dynamic_dofs(bool init = true)
    : dofs<G>(pos, vel, mom) {

    if(!init) return;
    
    std::fill(pos.begin(), pos.end(), traits<G>::id());
    std::fill(vel.begin(), vel.end(), traits< deriv<G> >::zero());
    std::fill(mom.begin(), mom.end(), traits< deriv<G> >::zero());                        
  }
 
};



#endif
