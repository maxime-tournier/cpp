
#include <boost/python.hpp>
#include <memory>

#include <iostream>
#include <vector>

struct A : std::enable_shared_from_this< A >{
  A() {
	std::cout << "A()" << std::endl;
  }

  static struct local{
	std::vector< std::shared_ptr<A> > instances;

	~local() {
	  std::cout << "~local" << std::endl;
	}
  } local; 

  static void push(std::shared_ptr<A> ptr) {
	local.instances.push_back(ptr);
  }
  
  std::size_t count() const {
	return shared_from_this().use_count() - 1;
  }
  
  ~A() {
	std::cout << "~A()" << std::endl;
  }
  
  std::string hello  () { return "Hello, is there anybody in there?"; }
};

struct A::local A::local;


using namespace boost::python;

// Declare the actual type
BOOST_PYTHON_MODULE(pointer) {
  class_<A, std::shared_ptr<A> >("A")
	.def("test", +[](A* self) {
		std::cout << "test" << std::endl;
	  })
	.def("count", &A::count)
	.def("push", &A::push)	
	
    ;
}

