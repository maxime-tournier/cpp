#include "node.hpp"

#include <boost/python.hpp>

#include "../func.hpp"


namespace python {

   void node::module() {
	
	using namespace boost::python;	

	class_<func_base, std::shared_ptr<func_base>, boost::noncopyable >("func_base", no_init);

   }

}
