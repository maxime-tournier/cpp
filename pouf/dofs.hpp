#ifndef DOFS_HPP
#define DOFS_HPP

#include "types.hpp"

// dofs
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
  
};

template<class G>
struct dofs : public dofs_base,
              public std::enable_shared_from_this< dofs<G> > {
  G pos;

  dofs_base::cast_type cast() {
    return this->shared_from_this();
  }

  std::size_t size() const {
    return 1;
  }
  
};





#endif
