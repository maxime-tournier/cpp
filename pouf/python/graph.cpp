#include "graph.hpp"

#include <core/graph.hpp>
#include <boost/python.hpp>

namespace python {

  void vertex::module() {
	using namespace boost::python;	

	using ::vertex;
	// TODO is_dofs, as_dofs, etc?
	class_<vertex>("vertex", no_init)
	  .def(init< std::shared_ptr<dofs_base> >())
	  .def(init< std::shared_ptr<func_base> >())
	  .def(init< std::shared_ptr<metric_base> >())
	  ;

	implicitly_convertible<std::shared_ptr<dofs_base>, vertex>();
	implicitly_convertible<std::shared_ptr<func_base>, vertex>();
	implicitly_convertible<std::shared_ptr<metric_base>, vertex>();
  }



  void graph::module() {
	using namespace boost::python;		  
	using ::graph;
	class_<graph, std::shared_ptr<graph> >("graph")
	
	  .def("add", &graph::add)
	
	  .def("__len__", +[](const graph& self) {
		  return num_vertices(self);
		})
	
	  .def("connect", &graph::connect)
	  .def("disconnect", &graph::disconnect)
	  ;

  }
  
}
