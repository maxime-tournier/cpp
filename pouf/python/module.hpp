#ifndef PYTHON_MODULE_HPP
#define PYTHON_MODULE_HPP


namespace python {

  void init_module();
  
  class registration {
  public:
	registration( void (*) () );
  };
  

  template<class T>
  struct module {

	static struct registration : python::registration {
	  registration() : python::registration([]{ T(); }) { }
	} instance;

	// forces initialization of instance
	// http://stackoverflow.com/a/6455451/1753545
	template<class U, U> struct value;
	using trigger = value<registration&, instance>;
  
	module() {
	  static unsigned once = ( (void) once, T::module(), 0);
	}

  };

  template<class T>
  typename module<T>::registration module<T>::instance;

}

#endif
