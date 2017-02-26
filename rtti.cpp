#include "rtti.hpp"

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


#include <iostream>

int main(int, char** ) { 
  base* x = new derived;
  base* y = new other;  
  
  std::cout << derived::classof(x) << std::endl;
  std::cout << other::classof(x) << std::endl;

  if( auto dx = rtti::cast<derived>(x) ) {
	std::cout << "it's alive: " << dx << std::endl;
  }

 
}
