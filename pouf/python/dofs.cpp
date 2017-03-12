#include "dofs.hpp"
#include "numpy.hpp"

#include "../dofs.hpp"

#include <boost/python.hpp>

namespace python {


  template<class T>
  static boost::python::object pos(::dofs< T >* self) {
	real* data = reinterpret_cast<real*>(self->pos.begin());
	numpy::shape_type shape = {(long)self->size(), T::RowsAtCompileTime};
	return numpy::ndarray(data, shape);
  }


  template<class T>
  static boost::python::object vel(::dofs< T >* self) {
	real* data = reinterpret_cast<real*>(self->vel.begin());
	numpy::shape_type shape = {(long)self->size(), T::RowsAtCompileTime};
	return numpy::ndarray(data, shape);
  }

  template<class T>
  static boost::python::object mom(::dofs< T >* self) {
	real* data = reinterpret_cast<real*>(self->mom.begin());
	numpy::shape_type shape = {(long)self->size(), T::RowsAtCompileTime};
	return numpy::ndarray(data, shape);
  }
  
  
  template<class T, boost::python::object (*get)(::dofs< T >*) >
  static void set(::dofs<T>* self, boost::python::object obj) {
	get(self)[ boost::python::slice() ] = obj;
  }
  
  
  void dofs::module() {
	// TODO automatic instantitations
	// dofs
	using namespace boost::python;		  
	using ::dofs;
	class_<dofs_base, std::shared_ptr<dofs_base>, boost::noncopyable >("dofs_base", no_init);	

	class_<dofs<vec3>, std::shared_ptr<dofs<vec3>>,
		   bases<dofs_base>, boost::noncopyable >("dofs_vec3", no_init)
	  .add_property("pos", pos<vec3>, set<vec3, pos<vec3> >)
	  .add_property("vel", vel<vec3>, set<vec3, vel<vec3> >)
	  .add_property("mom", mom<vec3>, set<vec3, mom<vec3> >)	  	  
	  ;
	
	
	// warning: replace class name
	class_<static_dofs<vec3>, std::shared_ptr<static_dofs<vec3>>,
		   bases<dofs<vec3>>, boost::noncopyable >("dofs_vec3")
	  ;
	
	
  }

}
