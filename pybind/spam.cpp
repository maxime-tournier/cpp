
#include <Python.h>
#include <memory>
#include <vector>

#include <iostream>

struct spam {
  PyObject* self = nullptr;
  
public:

  spam(PyObject* self)
	: self(self) {
	Py_XINCREF(self);
	std::clog << "> spam() c++: " << this << " / py: " << self << std::endl;
  }

  
  ~spam() {
	std::clog << "< ~spam() c++: " << this << " / py: " << self << std::endl;
	Py_XDECREF(self);
  }

  
  using ref = std::shared_ptr<spam>;

  std::string bacon() const {
    PyObject* obj = PyObject_CallMethod(self, "bacon", NULL);
    std::string res = PyString_AsString(obj);
    Py_XDECREF(obj);
    return res;
  }

  
};

static const struct sentinel {
  sentinel() { std::clog << "sentinel" << std::endl; }
  ~sentinel() { std::clog << "~sentinel" << std::endl; }  
} instance;




struct py_spam {
  PyObject_HEAD;
  spam::ref obj;

  static int tp_traverse(py_spam *self, visitproc visit, void *arg);
  static int tp_clear(py_spam *self);
  static PyObject* tp_new(PyTypeObject* cls, PyObject* args, PyObject* kwargs);

  static PyObject* bacon(PyObject* self, PyObject* args, PyObject* kwargs) {
    return PyString_FromString("standard bacon");
  }
  
  static PyMethodDef tp_methods[];
};

PyMethodDef py_spam::tp_methods[] = {
  {"bacon", (PyCFunction)bacon,  METH_KEYWORDS, "give the user some bacon"},
  {NULL}
};

static PyMethodDef module_methods[] = {
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,        /* tp_flags */
  "spam bacon and spam",           /* tp_doc */
};



#define AS_GC(o) ((PyGC_Head *)(o)-1)

static long& gc_refs(PyObject* self) {
  PyGC_Head *gc = AS_GC(self);
  return gc->gc.gc_refs;
}

// static std::size_t force_gc(PyObject* self) {
//   PyGC_Head *gc = AS_GC(self);
//   _PyGCHead_SET_REFS(gc, _PyGC_REFS_TENTATIVELY_UNREACHABLE);  
// }

int py_spam::tp_traverse(py_spam *self, visitproc visit, void *arg) {
  std::clog << "tp_traverse py: " << self << " / c++: " << self->obj << std::endl;
  std::clog << "  c++ use_count: " << self->obj.use_count() << std::endl;
  std::clog << "  py: gc_refs: " << gc_refs((PyObject*) self) << std::endl;  
  
  // std::clog << std::hex << gc_refs((PyObject*) self)  << std::endl;
  std::clog << "before" << std::endl;
  Py_VISIT(self);
  std::clog << "after" << std::endl;
  
  return 0;
}

int py_spam::tp_clear(py_spam *self) {
  std::clog << "tp_clear py: " << self << " / c++: " << self->obj << std::endl;
  self->obj.reset();
  return 0;
}

PyObject* py_spam::tp_new(PyTypeObject* cls, PyObject* args, PyObject* kwargs) {
  std::clog << "tp_new" << std::endl;
  PyObject* self = PyType_GenericAlloc(cls, 1);

  // allocate c++ object
  py_spam* ptr = reinterpret_cast<py_spam*>(self);
  ptr->obj = std::make_shared<spam>(self);

  return self;
}





PyMODINIT_FUNC initspam(void) {
  spam_type.tp_traverse = (traverseproc) py_spam::tp_traverse;
  spam_type.tp_clear = (inquiry) py_spam::tp_clear;  
  spam_type.tp_new = py_spam::tp_new;
  
  spam_type.tp_methods = py_spam::tp_methods;
  
  // noddy_NoddyType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&spam_type) < 0) return;

  PyObject* module = Py_InitModule3("spam", module_methods, 
                                    "Example module that creates an extension type.");

  
  Py_INCREF(&spam_type);
  PyModule_AddObject(module, "spam", (PyObject *)&spam_type);
}
