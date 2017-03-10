
#include <boost/python.hpp>

#include "graph.hpp"

#include "dofs.hpp"
#include "metric.hpp"
#include "func.hpp"

// Declare the actual type
BOOST_PYTHON_MODULE(pouf) {
  using namespace boost::python;


  
  class_<graph, std::shared_ptr<graph> >("graph")
	
	.def("add", &graph::add)
	
	.def("__len__", +[](const graph& self) {
		return num_vertices(self);
	  })
	
	.def("connect", &graph::connect)
	.def("disconnect", &graph::disconnect)
	
    ;
 

  class_<dofs_base, std::shared_ptr<dofs_base>, boost::noncopyable >("dofs_base", no_init);
  class_<metric_base, std::shared_ptr<metric_base>, boost::noncopyable >("metric_base", no_init);
  class_<func_base, std::shared_ptr<func_base>, boost::noncopyable >("func_base", no_init);  


  // TODO is_dofs, as_dofs, etc?
  class_<vertex>("vertex", no_init)
	.def(init< std::shared_ptr<dofs_base> >())
	.def(init< std::shared_ptr<func_base> >())
	.def(init< std::shared_ptr<metric_base> >())
	;

  implicitly_convertible<std::shared_ptr<dofs_base>, vertex>();
  implicitly_convertible<std::shared_ptr<func_base>, vertex>();
  implicitly_convertible<std::shared_ptr<metric_base>, vertex>();

  // TODO automatic instantitations
  // dofs 
  class_<dofs<vec3>, std::shared_ptr<dofs<vec3>>, bases<dofs_base> >("dofs_vec3");
  

  // metrics
  class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
  		  bases<metric_base> >("uniform_mass_vec3")
  	.def_readwrite("value", &uniform_mass<vec3>::value)
  	;
   

  // // functions
  // // class_< uniform_mass<vec3>, std::shared_ptr< uniform_mass<vec3>>,
  // // 		  bases<metric_base> >("uniform_mass_vec3");
  
   
}

