#include "metric.hpp"

#include <core/metric.hpp>

#include <boost/python.hpp>

#include "../uniform.hpp"
#include "../rigid_mass.hpp"

#include "numpy.hpp"

namespace python {


  template<class Metric>
  static void bind_uniform(const char* name) {
	using namespace boost::python;	
	class_< Metric, std::shared_ptr< Metric>,
			bases< metric_base >, boost::noncopyable >(name)
	  .def_readwrite("value", &Metric::value)
	  ;
  }



  
  
  void metric::module() {
	using namespace boost::python;
	using ::metric;
	
	class_<metric_base, std::shared_ptr<metric_base>, 
		   boost::noncopyable >("metric_base", no_init);

    // 
	class_< mass<vec3>, std::shared_ptr< mass<vec3>>,
			boost::noncopyable, bases<metric_base> >("mass_vec3", no_init);

	class_< mass<rigid>, std::shared_ptr< mass<rigid>>,
			boost::noncopyable, bases<metric_base> >("mass_rigid", no_init);

    
	// TODO: put this in uniform.cpp
	bind_uniform< uniform_mass<vec3> >("uniform_mass_vec3");
    bind_uniform< uniform_mass<rigid> >("uniform_mass_rigid");    


	bind_uniform< uniform_stiffness<vec3> >("uniform_stiffness_vec3");
	bind_uniform< uniform_stiffness<real> >("uniform_stiffness_real");
	
	bind_uniform< uniform_compliance<vec3> >("uniform_compliance_vec3");
	bind_uniform< uniform_compliance<real> >("uniform_compliance_real");


    static auto get = +[](rigid_mass<real>* self) {
      return numpy::ndarray(self->mass.begin(), { (npy_intp)self->size() } );
    };
    
    class_< rigid_mass<real>, std::shared_ptr< rigid_mass<real> >,
            boost::noncopyable, bases< mass<rigid> > > ("mass_rigid")
      .add_property("mass", get, +[](rigid_mass<real>* self, object obj) {
          get(self)[boost::python::slice()] = obj;
        })
      ;
    
  }
}
