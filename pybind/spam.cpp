
#include <Python.h>
#include <memory>
#include <vector>

#include <iostream>

template<class T>
using ptr = std::shared_ptr<T>;

struct base {
  PyObject* self = nullptr;
  
  base(PyObject* self) : self(self) {
    // the c++ object keeps the python object alive, which will keep the c++
    // object alive. only python gc can detect/break the cycle (see
    // tp_traverse/tp_clear).
    Py_XINCREF(self);
    std::clog << "base()" << std::endl;    
  }
  
  virtual ~base() {
    // the c++ object is expected to live as long as the python object, with
    // python controlling the release. the only legal way this object can be
    // freed is after tp_clear is called on the python object, which clears
    // 'self' for this object. hence: 'self' must be null.
    assert( !self && "python object outliving its c++ counterpart");
    std::clog << "~base()" << std::endl;
  }

  // python object
  struct object {
    PyObject_HEAD;
    ptr<base> obj;
  };

  // python methods
  static int tp_traverse(PyObject *self, visitproc visit, void *arg) {
    std::clog << "tp_traverse" << std::endl;
                                  
    // py_visit will notify python gc about the c++/python/c++ cycle: this is as
    // if this python objects acts like a container object containing
    // itself. when the python object is detected unreachable, tp_clear will be
    // called

    // we only allow python gc to collect this object when it becomes
    // unreachable from the c++ side too
    object* cast = reinterpret_cast<object*>(self);
    if(cast->obj.unique()) {
      Py_VISIT(self);
    }
    return 0;
  }

  static int tp_clear(PyObject *self) {
    std::clog << "tp_clear" << std::endl;
    
    object* cast = reinterpret_cast<object*>(self);
    // the python object became unreachable from the python side: it will be
    // collected. we notify the c++ object accordingly.
    cast->obj->self = nullptr;

    // we expect the c++ object to be released after this point, otherwise it
    // will outlive its python counterpart.
    assert( cast->obj.unique() && "c++ object outliving its python counterpart");
    cast->obj.reset();
    return 0;
  }

  // python type
  static PyTypeObject type;
  
};

PyTypeObject base::type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "base",             /* tp_name */
  sizeof(object), /* tp_basicsize */
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
  "base class for collectable objects",           /* tp_doc */
  tp_traverse,
  tp_clear,
};




struct spam : base {
  using base::base;
  
  std::string bacon() const {
    PyObject* obj = PyObject_CallMethod(self, (char*) "bacon", NULL);
    std::string res = PyString_AsString(obj);
    Py_XDECREF(obj);
    return res;
  }
  
  static PyObject* tp_new(PyTypeObject* cls, PyObject* args, PyObject* kwargs) {
    std::clog << "tp_new" << std::endl;
    PyObject* self = PyType_GenericAlloc(cls, 1);

    // allocate c++ object
    base::object* cast = reinterpret_cast<base::object*>(self);
    cast->obj = std::make_shared<spam>(self);

    // keep a handle on c++ objects
    static std::vector<ptr<base> > instances;
  
    return self;
  }

  static PyObject* py_bacon(PyObject* cls, PyObject* args, PyObject* kwargs) {
    return PyString_FromString("standard bacon");
  }
  
  static PyMethodDef tp_methods[];
};

PyMethodDef spam::tp_methods[] = {
  {"bacon", (PyCFunction)py_bacon,  METH_KEYWORDS, "give the user some bacon"},
  {NULL}
};


static const struct sentinel {
  sentinel() { std::clog << "sentinel" << std::endl; }
  ~sentinel() { std::clog << "~sentinel" << std::endl; }  
} instance;




static PyMethodDef module_methods[] = {
  {NULL}
};


static PyTypeObject spam_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "spam",             /* tp_name */
  sizeof(base::object), /* tp_basicsize */
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
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




PyMODINIT_FUNC initspam(void) {
  if (PyType_Ready(&base::type) < 0) return;
  Py_INCREF(&base::type);
  
  // spam_type.tp_traverse = (traverseproc) py_spam::tp_traverse;
  // spam_type.tp_clear = (inquiry) py_spam::tp_clear;  

  spam_type.tp_new = spam::tp_new;
  spam_type.tp_methods = spam::tp_methods;
  spam_type.tp_base = &base::type;  
  
  // noddy_NoddyType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&spam_type) < 0) return;

  PyObject* module = Py_InitModule3("spam", module_methods, 
                                    "Example module that creates an extension type.");
  
  Py_INCREF(&spam_type);

  PyModule_AddObject(module, "spam", (PyObject *)&spam_type);
}
