#include "dofs.hpp"

#include "../dofs.hpp"
#include <boost/python.hpp>

namespace python {

  void dofs::module() {
	// TODO automatic instantitations
	// dofs
	using namespace boost::python;		  
	using ::dofs;
	class_<dofs_base, std::shared_ptr<dofs_base>, boost::noncopyable >("dofs_base", no_init);	
	class_<dofs<vec3>, std::shared_ptr<dofs<vec3>>, bases<dofs_base> >("dofs_vec3");
  }

}
