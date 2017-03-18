#include "dofs.hpp"
#include "numpy.hpp"

#include <core/dofs.hpp>

#include <boost/python.hpp>

namespace python {


  template<class T>
  static npy_intp size() {
    static_assert( std::is_same<real, scalar< deriv<T> > >::value, "need real scalar type");
    return sizeof(T) / sizeof(real);
  }
  
  template<class T>
  static boost::python::object pos(::dofs< T >* self) {
	real* data = reinterpret_cast<real*>(self->pos.begin());
	numpy::shape_type shape = {(long)self->size(), size<T>()  };
	return numpy::ndarray(data, shape);
  }


  template<class T>
  static boost::python::object vel(::dofs< T >* self) {
	real* data = reinterpret_cast<real*>(self->vel.begin());
	numpy::shape_type shape = {(long)self->size(), size<deriv<T>>()};
	return numpy::ndarray(data, shape);
  }

  template<class T>
  static boost::python::object mom(::dofs< T >* self) {
	real* data = reinterpret_cast<real*>(self->mom.begin());
	numpy::shape_type shape = {(long)self->size(), size<deriv<T>>()};
	return numpy::ndarray(data, shape);
  }
  
  
  template<class T, boost::python::object (*get)(::dofs< T >*) >
  static void set(::dofs<T>* self, boost::python::object obj) {
	get(self)[ boost::python::slice() ] = obj;
  }
  

  namespace impl {
    template<class U>
    static void bind_dofs(const std::string& tname) {
      using namespace boost::python;		 

      // static copy
      static std::string name = tname;

      // std::clog << name << std::endl;
      
      // TODO namespace
      using ::dofs;
    
      class_<dofs<U>, std::shared_ptr<dofs<U>>,
             bases<dofs_base>, boost::noncopyable >(name.c_str(), no_init)
        .add_property("pos", pos<U>, set<U, pos<U> >)
        .add_property("vel", vel<U>, set<U, vel<U> >)
        .add_property("mom", mom<U>, set<U, mom<U> >)	  	  
        ;

      // warning: replace class name    
      class_<static_dofs<U>, std::shared_ptr<static_dofs<U>>,
             bases<dofs<U>>, boost::noncopyable >_(name.c_str()) // most vexing parse
        ;

    
    }

  }


  template<class ... U>
  static void bind_dofs() {
    std::string prefix = "dofs_";
    const int expand[] = { (impl::bind_dofs<U>(prefix + traits<U>::name()), 0)... };
    (void) expand;
  }
  
  void dofs::module() {
	// TODO automatic instantitations
	// dofs

	using namespace boost::python;		  
	using ::dofs;
	class_<dofs_base, std::shared_ptr<dofs_base>, boost::noncopyable >("dofs_base", no_init);	

    bind_dofs<real, vec3, rigid>();


    dynamic_dofs<vec3> test;
  }

}
