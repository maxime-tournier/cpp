#ifndef PYTHON_GRAPH_HPP
#define PYTHON_GRAPH_HPP

#include "dofs.hpp"
#include "metric.hpp"
#include "func.hpp"

namespace python {

  
  struct vertex : dofs, func, metric, module<vertex> {
	static void module();
  };
  
  struct graph : vertex, module<graph> {
	static void module();
  };

}

#endif
