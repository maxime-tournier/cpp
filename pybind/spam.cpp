
#include <Python.h>
#include <memory>
#include <vector>

class spam {
  PyObject* self = nullptr;


  
public:

  spam(PyObject* self)
	: self(self) {
	Py_INCREF(self);

  }

  
  ~spam() {
	Py_DECREF(self);
	std::clog << "~spam" << std::endl;
  }

  
  using ref = std::shared_ptr<spam>;
  
  
  void bacon() {
    PyObject* res = PyObject_CallMethod(self, "bacon", NULL);
    Py_DECREF(res);
  }

  static std::vector<ref> instances;
  
};



struct py_spam {
  PyObject_HEAD;
  spam::ref obj;


  static PyObject* gimme(PyObject* self, PyObject* args);
  
};

static PyMethodDef spam_methods[] = {
  {"gimme",  py_spam::gimme, METH_VARARGS,
   "produce unlimited spam"},  
  {NULL}  /* Sentinel */
};


static PyTypeObject spam_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "spam.spam",             /* tp_name */
  sizeof(py_spam), /* tp_basicsize */
  0,                         /* tp_itemsize */
  0,                         /* tp_dealloc */
  0,                         /* tp_print */
  0,                         /* tp_getattr */
  0,                         /* tp_setattr */
  0,                         /* tp_compare */
  0,                         /* tp_repr */
  0,                         /* tp_as_number */
  0,                         /* tp_as_sequence */
  0,                         /* tp_as_mapping */
  0,                         /* tp_hash */
  0,                         /* tp_call */
  0,                         /* tp_str */
  0,                         /* tp_getattro */
  0,                         /* tp_setattro */
  0,                         /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,        /* tp_flags */
  "spam bacon and spam",           /* tp_doc */
};


PyObject* py_spam::gimme(PyObject*, PyObject* args) {

  if(!PyArg_ParseTuple(args, "")) return NULL;
  
  PyObject* self = PyType_GenericNew(&spam_type, args, NULL);
  py_spam* py = reinterpret_cast<py_spam*>(self);

  // gc cycle here
  py->obj = std::make_shared<spam>(self);

  return self;
}



PyMODINIT_FUNC
initspam(void) 
{
  // PyObject* m;
  
  // noddy_NoddyType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&spam_type) < 0) return;

  PyObject* module = Py_InitModule3("spam", spam_methods, 
                                    "Example module that creates an extension type.");

  Py_INCREF(&spam_type);
  PyModule_AddObject(module, "spam", (PyObject *)&spam_type);
}
