
#include <Python.h>
#include <memory>
#include <vector>

#include <iostream>

struct spam {
  PyObject* self = nullptr;
  
public:

  spam(PyObject* self)
	: self(self) {
	Py_INCREF(self);
	std::clog << "creating spam" << std::endl;
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


std::vector<spam::ref> spam::instances;

// module methods
static PyObject* some(PyObject* self, PyObject* args);
static PyObject* clear(PyObject* self, PyObject* args);  

struct py_spam {
  PyObject_HEAD;
  spam::ref obj;

  static void dealloc(py_spam* self);
  static int traverse(py_spam *self, visitproc visit, void *arg);
  static int clear(py_spam *self);
};

static PyMethodDef module_methods[] = {
  {"some",  some, METH_VARARGS, "produce a limited quantity of spam"},
  {"clear",  clear, METH_VARARGS, "clear all spam in the universe"},    
  {NULL}
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,        /* tp_flags */
  "spam bacon and spam",           /* tp_doc */
};


PyObject* some(PyObject*, PyObject* args) {

  if(!PyArg_ParseTuple(args, "")) return NULL;
  
  PyObject* self = PyType_GenericNew(&spam_type, args, NULL);
  py_spam* py = reinterpret_cast<py_spam*>(self);

  // gc cycle here
  py->obj = std::make_shared<spam>(self);

  // add ourselves to instance list
  spam::instances.push_back(py->obj);
  
  return self;
}


PyObject* clear(PyObject*, PyObject* args) {
  
  if(!PyArg_ParseTuple(args, "")) return NULL;

  spam::instances.clear();
  
  Py_RETURN_NONE;
}

void py_spam::dealloc(py_spam* self) {
  std::clog << "dealloc" << std::endl;  
  self->obj.reset();
  clear(self);
  self->ob_type->tp_free((PyObject*)self);
}

#define AS_GC(o) ((PyGC_Head *)(o)-1)

static std::size_t gc_refs(PyObject* self) {
  PyGC_Head *gc = AS_GC(self);
  return gc->gc.gc_refs;
}

int py_spam::traverse(py_spam *self, visitproc visit, void *arg) {
  std::clog << "traverse" << std::endl;

  std::clog << gc_refs((PyObject*) self)  << std::endl;
  Py_VISIT(self->obj->self);

  std::clog << gc_refs((PyObject*) self)  << std::endl;  
  return 0;
}

int py_spam::clear(py_spam *self) {
  std::clog << "clear" << std::endl;
  Py_CLEAR(self->obj->self);
  return 0;
}


PyMODINIT_FUNC initspam(void) {
  
  spam_type.tp_dealloc = (destructor) py_spam::dealloc;
  spam_type.tp_traverse = (traverseproc) py_spam::traverse;

  // noddy_NoddyType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&spam_type) < 0) return;

  PyObject* module = Py_InitModule3("spam", module_methods, 
                                    "Example module that creates an extension type.");

  Py_INCREF(&spam_type);
  PyModule_AddObject(module, "spam", (PyObject *)&spam_type);
}
