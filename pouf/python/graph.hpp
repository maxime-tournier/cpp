#ifndef PYTHON_GRAPH_HPP
#define PYTHON_GRAPH_HPP

#include "dofs.hpp"
#include "node.hpp"

namespace python {

  
  struct vertex : dofs, node, module<vertex> {
	static void module();
  };
  
  struct graph : vertex, module<graph> {
	static void module();
  };

}

#endif
