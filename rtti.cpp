#include "rtti.hpp"

#include <iostream>

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
				public rtti::derived<derived> {
  
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