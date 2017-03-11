
#include <boost/python.hpp>

#include "graph.hpp"

#include "dofs.hpp"
#include "metric.hpp"
#include "func.hpp"

#include "typecheck.hpp"

#include "python/module.hpp"


  
  // // metrics
  // class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
  // 		  bases<metric_base> >("uniform_mass_vec3")
  // 	.def_readwrite("value", &uniform_mass<vec3>::value)
  // 	;
   

  // // functions
  // // class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
  // // 		  bases<metric_base> >("uniform_mass_vec3");


// // Declare the actual type
BOOST_PYTHON_MODULE(pouf) {
  python::init_module();
}

