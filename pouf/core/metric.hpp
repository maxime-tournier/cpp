#ifndef POUF_METRIC_HPP
#define POUF_METRIC_HPP

#include <memory>

#include "types.hpp"
#include "vec.hpp"
#include "real.hpp"
#include "rigid.hpp"

#include "slice.hpp"
#include "sparse.hpp"
#include "api.hpp"


// metrics
template<class G>
struct metric;

struct metric_base {
  virtual ~metric_base() { }

  using cast_type = api< 
    metric<real>,
    metric<vec3>,
    metric<rigid>>;

  const cast_type cast;
  
protected:

  template<class Derived>
  metric_base(Derived* self)
	: cast(self, &metric_base::cast) {
	assert(self == this);
  };
};

template<class G> struct mass;
template<class G> struct stiffness;
template<class G> struct damping;
template<class G> struct compliance;


// a symmetric tensor ranging over a space
template<class G>
struct metric : metric_base {

  using metric_base::metric_base;

  using position_type = G;
  
  virtual void tensor(triplet_iterator out, slice<const G> at) const = 0;
  
  const api< mass<G>,
			 stiffness<G>,
			 damping<G>,
			 compliance<G> > cast;

  enum {
	mass_type,
	stiffness_type,
	damping_type,
	compliance_type
  };

  
protected:
  
  template<class Derived>
  metric(Derived* self)
	: metric_base(this),
	  cast(self, &metric::cast) {
	assert(self == this);
  }
  
};



template<class G>
struct mass : metric<G> {

  mass() : metric<G>(this)  { }
  
  virtual void momentum(slice< deriv<G> > out, slice<const G> pos, slice<const deriv<G> > vel) const = 0;

  virtual void force(slice< deriv<G> > out, 
                     slice<const G> pos, slice<const deriv<G> > vel,
                     const vec3& g) const = 0;
};


template<class G>
struct stiffness : metric<G> {

  stiffness() : metric<G>(this)  { }
  
  virtual void gradient(slice< deriv<G> > out, slice<const G> pos) const = 0;
  
};


template<class G>
struct damping : metric<G> {

  damping() : metric<G>(this)  { }

  // TODO force ?
};


template<class G>
struct compliance : metric<G> {
  compliance() : metric<G>(this)  { }

  // TODO error ?
};






#endif
