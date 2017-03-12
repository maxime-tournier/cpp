#ifndef FUNC_HPP
#define FUNC_HPP

#include <memory>
#include "../variant.hpp"
#include "vec.hpp"
#include "real.hpp"
#include "sparse.hpp"
#include "slice.hpp"

// functions
template<class T>
struct func;

struct func_base {
  virtual ~func_base() { }

  using cast_type = variant< 
    func<real (real) >,
    func<real (vec3)>,
    func<real (vec3, vec2)>
    >;
  
  virtual cast_type cast() = 0;

};



template<class, class T>
using repeat = T;


template<class To, class ... From>
struct func< To (From...) > : public func_base,
                              public std::enable_shared_from_this< func< To (From...) > >{

  func_base::cast_type cast() {
    return this->shared_from_this();
  }
  
  // output size from inputs
  virtual std::size_t size(slice<const From>... from) const = 0;

  // apply function
  virtual void apply(slice<To> to, slice<const From>... from) const = 0;

  // sparse jacobian
  virtual void jacobian(repeat<From, triplet_iterator> ... out,
                        slice<const From> ... from) const = 0;
  
  // sparse hessian
  virtual void hessian(triplet_iterator out,
                       slice< const deriv<To> >& dto,
                       slice< const From> ... from) const { }
};


#endif
