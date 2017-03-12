#include "func.hpp"

#include "../func.hpp"

#include <boost/python.hpp>

namespace python {


  // TODO this should go somewhere else
  template<class U>
  struct norm2 : ::func< scalar<U> ( U ) > {

	using To = scalar<U>;
	using From = U;

	using dTo = deriv<To>;
	using dFrom = deriv<From>;
  
	virtual std::size_t size( slice<const From> ) const {
	  return 1;
	}
  
	virtual void apply( slice<To> to, slice<const From> from) const {
	  to[0] = 0;

	  for(const From& x : from) {
		to[0] += traits<From>::dot(x, x);
	  }
	  
	  to[0] /= 2.0;
	}
  

	virtual void jacobian(triplet_iterator block, slice<const From> from) const {

	  unsigned off = 0;

	  for(const From& x : from) {
		for(unsigned j = 0, m = traits<From >::dim; j < m; ++j, ++off) {
		  *block++ = triplet(0, off, traits<From>::coord(j, x));
		}
	  }
	  
	}


	virtual void hessian(triplet_iterator block, 
						 slice< const dTo > lambda, slice< const dFrom > from) const {
	  for(int i = 0, n = from.size() * traits< From >::dim; i < n; ++i) {
		*block++ = triplet(i, i, lambda[0]);
	  }
	}
  
  };
  
  
  void func::module() {
	using namespace boost::python;
	using ::func;

	class_<func_base, std::shared_ptr<func_base>, boost::noncopyable >("func_base", no_init);

	// TODO moar
	class_<norm2<vec3>, std::shared_ptr<norm2<vec3>>,
		   bases<func_base>, boost::noncopyable >("norm2_vec3");
  }

}
