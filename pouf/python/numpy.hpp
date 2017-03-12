#ifndef PYTHON_NUMPY_HPP
#define PYTHON_NUMPY_HPP

#include <boost/python.hpp>

// lolwat
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/ndarrayobject.h>

#include "module.hpp"

namespace python {


  struct numpy : module<numpy> {

	static void module();

	using shape_type = std::initializer_list<npy_intp>;

	// wrap c++ array as numpy array	
	static boost::python::object ndarray(double* data, shape_type shape);

	// silence warning
	template<class T, T> struct value;
	using trigger = value< int (*)(), _import_array>;
  };
  
  
}



#endif
