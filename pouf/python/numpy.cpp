#include "numpy.hpp"



namespace python {


  void numpy::module() {
	// numpy requires this
    [] {
      import_array();
    }();
  }

  boost::python::object numpy::ndarray(double* data, shape_type shape) {
	using namespace boost::python;

	PyObject* obj = PyArray_New(&PyArray_Type, shape.size(),
								const_cast<npy_intp*>(shape.begin()), NPY_DOUBLE, // data type
								NULL, data, // data pointer
								0, NPY_ARRAY_CARRAY, // NPY_ARRAY_CARRAY_RO for readonly
								NULL);
	handle<> array( obj );
	return object(array);
  }
  
}
