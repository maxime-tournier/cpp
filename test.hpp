#ifndef TEST_HPP
#define TEST_HPP

#include "rtti.hpp"

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
  derived(); 
};


class other : public base,  
			  public rtti::derived<other> {
  
public:
  other(); 
}; 



#endif
