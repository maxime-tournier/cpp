#ifndef POUF_METRIC_HPP
#define POUF_METRIC_HPP

#include <memory>

#include "vec.hpp"
#include "real.hpp"
#include "slice.hpp"
#include "sparse.hpp"
#include "../variant.hpp"

// metrics
template<class G>
struct metric;

enum class metric_kind {
  mass,
  damping,
  stiffness,
  compliance
};


struct metric_base {
  virtual ~metric_base() { }

  using cast_type = variant< 
    metric<real>,
    metric<vec1>,
    metric<vec2>,
    metric<vec3>,
    metric<vec4>,
    metric<vec6>>;

  virtual cast_type cast() = 0;

  const metric_kind kind;
  
  metric_base(metric_kind kind) : kind(kind) { };
};


template<class G>
struct metric : metric_base, std::enable_shared_from_this< metric<G> > {

  using metric_base::metric_base;
  
  typename metric::cast_type cast() { return this->shared_from_this(); }
  virtual void tensor(triplet_iterator out, slice<const G> at) const = 0;
  
};



template<metric_kind kind, class G>
struct uniform : metric<G> {

  uniform(real value = 1.0)
    : metric<G>(kind),
    value(value) {
	
  }
  
  real value;
  
  virtual void tensor(triplet_iterator out, slice<const G> at) const {
	std::clog << "uniform at size: " << at.size() << std::endl;
    for(int i = 0, n = at.size() * traits<G>::dim; i < n; ++i) {
      *out++ = {i, i, value};
    }
  }
	
};




template<class G>
using uniform_mass = uniform<metric_kind::mass, G>;

template<class G>
using uniform_stiffness = uniform<metric_kind::stiffness, G>;

template<class G>
using uniform_damping = uniform<metric_kind::damping, G>;

template<class G>
using uniform_compliance = uniform<metric_kind::compliance, G>;




#endif
