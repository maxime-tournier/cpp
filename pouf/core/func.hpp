#ifndef FUNC_HPP
#define FUNC_HPP

#include <memory>

#include "vec.hpp"
#include "real.hpp"
#include "sparse.hpp"
#include "slice.hpp"

#include "api.hpp"

// functions
template<class T>
struct func;


struct func_base {
  virtual ~func_base() { }

  using cast_type = api< 
    func<real (real) >,
    func<real (vec3)>,
    func<real (vec3, vec2)>
    >;

  const cast_type cast;

protected:
  template<class Derived>
  func_base(Derived* self)
	: cast(self, &func_base::cast) {
	assert(self == this);
  }
};



template<class, class T>
using repeat = T;


template<class To, class ... From>
struct func< To (From...) > : public func_base {

  func() : func_base(this) { }
  
  // output size from inputs
  virtual std::size_t size(slice<const From>... from) const = 0;

  // apply function
  virtual void apply(slice<To> to, slice<const From>... from) const = 0;

  // sparse jacobian
  virtual void jacobian(repeat<From, triplet_iterator> ... out,
                        slice<const From> ... from) const = 0;
  
  // sparse hessian
  virtual void hessian(triplet_iterator out,
                       slice< const deriv<To> > dto,
                       slice< const From> ... from) const { }
};


#endif
