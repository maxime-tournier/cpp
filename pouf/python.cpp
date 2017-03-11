
#include <boost/python.hpp>

#include "python/module.hpp"

#include "python/graph.hpp"
#include "simulation.hpp"

  // // functions
  // // class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
  // // 		  bases<metric_base> >("uniform_mass_vec3");

namespace python {

  struct simulation : graph, module<simulation> {
	static void module() {
	  using namespace boost::python;
	  using ::simulation;
	  
	  class_<simulation, std::shared_ptr<simulation> > ("simulation")
		.def("step", &simulation::step)
		;
	  
	}
  };

}



// // Declare the actual type
BOOST_PYTHON_MODULE(pouf) {
  python::init_module();
}

