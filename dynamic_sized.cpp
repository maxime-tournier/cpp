#include <iostream>

#include "dynamic_sized.hpp"


 
struct foo { 

  foo() {
    std::clog << "foo()" << std::endl;
  }
  
  foo(const foo& other) {
    std::clog << "foo(const foo&)" << std::endl;
  }

  ~foo() {
    std::clog << "~foo()" << std::endl;
  }
  
  
};
 

struct base { 
  virtual ~base() { };
};
 
  
struct derived : base, dynamic_sized<foo, derived> {
  int michel;
  // using dynamic_sized<foo>::operator new;
};

   
  
   

int main(int argc, char** argv) {

  // dynamic_sized<foo> test;
  
  base* ptr = new(4) derived;
  
  delete ptr;
    
  
  return 0;
}
