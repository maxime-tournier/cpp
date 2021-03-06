
#include <boost/python.hpp>

#include "python/module.hpp"

#include "python/graph.hpp"
#include "simulation.hpp"

  // // functions
  // // class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
  // // 		  bases<metric_base> >("uniform_mass_vec3");

namespace python {

  // TODO move ?where ?!
  struct simulation : graph, module<simulation> {
	static void module() {
	  using namespace boost::python;
	  using ::simulation;


      static auto get_gravity = +[](simulation* self) {
        return numpy::ndarray(self->gravity.data(), {3});
      };

      static auto set_gravity = +[](simulation* self, object obj) {
        get_gravity(self)[boost::python::slice()] = obj;
      };

      
	  class_<simulation, std::shared_ptr<simulation> > ("simulation")
		.def("step", &simulation::step)
		.def("init", &simulation::init)
		.def("reset", &simulation::reset)
        .add_property("gravity", get_gravity, set_gravity)
		;
	  
	}
  };

}



// // Declare the actual type
BOOST_PYTHON_MODULE(pouf) {
  python::init_module();
}

