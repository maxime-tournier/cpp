
#include <Python.h>
#include <memory>
#include <vector>

#include <iostream>
#include <boost/intrusive_ptr.hpp>

template<class T>
using ptr = boost::intrusive_ptr<T>;

#define AS_GC(o) ((PyGC_Head *)(o)-1)
 
static long& gc_refs(PyObject* self) {
  PyGC_Head *gc = AS_GC(self);
  return gc->gc.gc_refs;
}


struct py_base {
  std::size_t rc = 0;
  PyObject* self = nullptr;

  py_base(PyObject* self) : self(self) {
    // c++ object keeps python object alive
    Py_XINCREF(self);
    std::clog << "py_base() " << this << " - " << self << std::endl;
  }

  ~py_base() {
    std::clog << "~py_base() " << this << " - " << self << std::endl;    
    if(self) {
      // python ref should have been cleared *before* destruction happens
      // (during gc clear), or we may still have python ref pointing to the c++
      // object
      throw std::logic_error("attempting to destruct a c++ object with non-zero python ref");
    }
  }

  template<class Derived>
  friend void intrusive_ptr_add_ref(Derived* self) {
    ++self->rc;
  }

  template<class Derived>
  friend void intrusive_ptr_release(Derived* self) {
    std::clog << "decref: " << self << " " << self->rc << std::endl;
    if(--self->rc == 0) {
      delete self;
      return;
    }

  }
  
  
};



// a c++ class extensible from python
struct spam : py_base {
  
public:

  using py_base::py_base;
  
  std::string bacon() {
    PyObject* obj = PyObject_CallMethod(self, (char*)"bacon", NULL);

    // TODO check obj is actually a string
    std::string res = PyString_AsString(obj);
    Py_XDECREF(obj);
    
    return res;
  }

  
};

static struct test { 

  test() {
    std::clog << "test" << std::endl;
  }

  ~test() {
    std::clog << "~test" << std::endl;
  }

  
  std::vector< ptr<spam> > spams;
  
} instance;

// module methods
static PyObject* some(PyObject* self, PyObject* args);
static PyObject* clear(PyObject* self, PyObject* args);  

struct py_spam {
  PyObject_HEAD;
  ptr<spam> obj;

  static void dealloc(py_spam* self);
  static int traverse(py_spam *self, visitproc visit, void *arg);
  static int tp_clear(py_spam *self);


  static PyObject* bacon(py_spam* self, PyObject* args, PyObject** kwargs) {
    return PyString_FromString("standard bacon");
  }

  static PyObject* tp_new(PyTypeObject* cls, PyObject* args, PyObject* kwargs) {
    std::clog << "tp_new" << std::endl;
    PyObject* self = PyType_GenericAlloc(cls, 1);
    
    py_spam* py = reinterpret_cast<py_spam*>(self);

    // allocate c++ object here
    py->obj = new spam(self);
    instance.spams.emplace_back(py->obj);
    
    return self;
  }
  
};

static PyMethodDef module_methods[] = {
  {"some",  some, METH_VARARGS, "produce a limited quantity of spam"},
  {"clear",  clear, METH_VARARGS, "clear all known spam instances"},    
  {NULL}
};

static PyMethodDef spam_methods[] = {
  {"bacon", (PyCFunction)py_spam::bacon, METH_KEYWORDS,
   "gimme some bacon"
  },
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,        /* tp_flags */
  "spam bacon and spam",           /* tp_doc */
};


// implementation of module methods
static PyObject* some(PyObject*, PyObject* args) {

  if(!PyArg_ParseTuple(args, "")) return NULL;
  
  PyObject* self = PyType_GenericNew(&spam_type, args, NULL);
  py_spam* py = reinterpret_cast<py_spam*>(self);

  // gc cycle here
  py->obj = new spam(self);

  return self;
}


static PyObject* clear(PyObject*, PyObject* args) {
  if(!PyArg_ParseTuple(args, "")) return NULL;
  instance.spams.clear();
  std::clog << "spam instances cleared" << std::endl;
  Py_RETURN_NONE;
}



int py_spam::traverse(py_spam *self, visitproc visit, void *arg) {
  std::clog << "tp_traverse: " << self << std::endl;

  // 
  if( self->obj->rc == 1 ) Py_VISIT(self);
  else std::clog << "existing c++ refs outside python object, don't clear python object" << std::endl;
  return 0;
}

int py_spam::tp_clear(py_spam *self) {
  std::clog << "tp_clear: " << self << std::endl;

  // object is in a cycle: il will get collected. we notify c++ about it, and
  // prevent it from decref'ing during destruction, if any
  std::clog << " c++ rc: " << self->obj->rc << std::endl;
  self->obj->self = nullptr;
  self->obj.reset();
  
  return 0;
}


PyMODINIT_FUNC initspam(void) {
  
  // spam_type.tp_dealloc = (destructor) py_spam::dealloc;
  spam_type.tp_traverse = (traverseproc) py_spam::traverse;
  spam_type.tp_clear = (inquiry) py_spam::tp_clear;
  spam_type.tp_methods = spam_methods;
  spam_type.tp_new = py_spam::tp_new;
  
  // noddy_NoddyType.tp_new = PyType_GenericNew;
  if (PyType_Ready(&spam_type) < 0) return;

  PyObject* module = Py_InitModule3("spam", module_methods, 
                                    "Example module that creates an extension type.");

  Py_INCREF(&spam_type);
  
  PyModule_AddObject(module, "spam", (PyObject *)&spam_type);
}
