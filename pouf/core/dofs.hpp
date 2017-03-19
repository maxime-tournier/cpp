#ifndef DOFS_HPP
#define DOFS_HPP

#include "vec.hpp"
#include "real.hpp"
#include "rigid.hpp"

#include "slice.hpp"

#include "api.hpp"
#include <vector>

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

  // TODO do we want refs instead?
  coord_slice pos;
  deriv_slice vel;
  deriv_slice mom;
  
  dofs(coord_slice pos,
	   deriv_slice vel,
	   deriv_slice mom)
	: dofs_base(this),
	  pos(pos),
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

  static_dofs()
	: dofs<G>({storage.pos.begin(), storage.pos.end()},
              {storage.vel.begin(), storage.vel.end()},
              {storage.mom.begin(), storage.mom.end()}) {
    
  }
  
  std::size_t size() const { return N; }
};

template<class G>
struct dynamic_dofs : dofs<G> {
  
  union storage_type {
    struct fixed_type {
      G pos;
      deriv<G> vel, mom;
    } fixed;

    struct dynamic_type {
      std::vector<G> pos;
      std::vector< deriv<G> > vel, mom;
    } dynamic;

    storage_type() : fixed () { }
    ~storage_type() { }
    
  } storage;

  std::size_t size() const { return dofs<G>::pos.size(); }

  dynamic_dofs() :
    dofs<G>({&storage.fixed.pos, &storage.fixed.pos + 1},
            {&storage.fixed.vel, &storage.fixed.vel + 1},
            {&storage.fixed.mom, &storage.fixed.mom + 1}),
    
    storage() { }

  ~dynamic_dofs() {
    if(size() > 1) {
      storage.dynamic.~dynamic_type();
    } else {
      storage.fixed.~fixed_type();
    }
  }


  void resize(std::size_t n) {
    assert(n > 0);
    
    const std::size_t s = size();

    if(s == 1 && n > 1) {
      // fixed -> dynamic
      storage.fixed.~fixed_type();
      new (&storage.dynamic) typename storage_type::dynamic_type(n);
    } else if(s > 1 && n == 1) {
      // dynamic -> fixed
      storage.dynamic.~dynamic_type();
      new (&storage.fixed) typename storage_type::fixed_type;
    } else if (s > 1 && n > 1) {
      // dynamic -> dynamic 
      storage.dynamic.resize(n);
    } else {
      // static -> static
      assert( s == 1 && n == 1 );
    }
    
  }
  
};



#endif
