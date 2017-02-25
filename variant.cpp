#include "variant.hpp"


struct A { };
struct B { };
struct C { };
struct D : A { };


int main(int, char** ){

  variant<A, B> v = std::make_shared<A>();
  const variant<A, B> w = std::make_shared<D>();
  const variant<A, B> u = std::make_shared<B>();

  // const variant<A, B> z = std::make_shared<C>();
  
  struct visitor {

	void operator()(const A* a, std::string msg) const {
	  std::cout << msg << "const a" << std::endl;
	}

	  void operator()(A* a, std::string msg) const {
		std::cout << msg << "a" << std::endl;
	}

	  
	void operator()(const B* b, std::string msg) const {
	  std::cout << msg << "const b" << std::endl;	  
	}

	void operator()(B* b, std::string msg) const {
	  std::cout << msg << "b" << std::endl;	  
	}

	
  } vis;

	v.apply( vis, "hello " );	
	u.apply( vis, "derp " );
	w.apply( vis, "bob " );
	
}
