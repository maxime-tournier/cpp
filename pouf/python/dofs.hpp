#ifndef PYTHON_DOFS_HPP
#define PYTHON_DOFS_HPP

#include "module.hpp"
#include "numpy.hpp"

namespace python {

  struct dofs : numpy, module<dofs> {
	static void module();
  };

}


#endif
