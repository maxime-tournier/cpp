#include <iostream>
#include "rtti.hpp"

#include <iostream>
#include <vector>

int main(int, char** ) {

  // basic tests
  class base : public rtti::base<base> {
  
  public:
	//  

  protected:
	using rtti::base<base>::base;
  };
 

  class derived : public base, 
				  public rtti::derived<derived> {
 
  public:
	derived() : base(this) { }
  };


  class other : public base, 
				public rtti::derived<other> {
  
  public:
	other() : base(this) { }
  }; 


  base* x = new derived;
  base* y = new other;   
  
  std::cout << derived::classof(x) << std::endl;
  std::cout << other::classof(x) << std::endl;

  if( auto cast = rtti::cast<derived>(x) ) {
	std::cout << "it's a derived: " << cast << std::endl;
  }

  std::vector<base*> test;

  for(unsigned i = 0; i < 30000000; ++i) {

	base* obj = std::rand() % 2 ? (base*) new derived : (base*) new other;
	test.push_back( obj );
  }
	
  int cd = 0, co = 0;

  for(base* obj : test) {

	if(rtti::isa<derived>(obj) ) {
	  cd += 1;
	}

	if(rtti::isa<other>(obj) ) {
	  co += 1;
	}
  }
  
  // multiple inheritance
  struct A : rtti::base<A> {
  protected:
	using base::base;

  };

  struct B : rtti::base<B> {
  protected:
	using base::base;
  };


  struct C : A, B, rtti::derived<C> {
	C() : A(this), B(this) { }
  };

  
  C* c = new C;
  A* a = c;
  B* b = c;

  
  if( auto cast = rtti::cast<C>(a) ) {
	std::cout << "C" << std::endl;
  }

  if( auto cast = rtti::cast<C>(b) ) {
	std::cout << "C" << std::endl;
  }

  return 0;
}
