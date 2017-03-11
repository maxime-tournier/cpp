#include "metric.hpp"

#include "../metric.hpp"
#include <boost/python.hpp>

namespace python {

  void metric::module() {
	using namespace boost::python;
	using ::metric;
	
	class_<metric_base, std::shared_ptr<metric_base>, 
		   boost::noncopyable >("metric_base", no_init);

	// class_< mass<vec3>, std::shared_ptr< mass<vec3>>,
	// 		bases<metric_base> >("mass_vec3", no_init);
	
	  
	
	// TODO: put this in uniform.cpp
	// metrics
	class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
			bases< metric_base > >("uniform_mass_vec3")
	  .def_readwrite("value", &uniform_mass<vec3>::value)
	  ;
  
	

  }


}
