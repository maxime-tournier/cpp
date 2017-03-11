#ifndef PYTHON_NODE_HPP
#define PYTHON_NODE_HPP

#include "module.hpp"

namespace python {

  struct node : module<node> {
	static void module();
  };


}


#endif
