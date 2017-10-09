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
 
  
struct derived : base, dynamic_sized<foo> {
  const char* name;
  derived(const char* name) : name(name) { }
  derived() = delete;
};

   
  
   

int main(int argc, char** argv) {

  base* ptr = derived::create<derived>(4, "foo");
  
  delete ptr;
    
  
  return 0;
}
