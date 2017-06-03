#include "gc.hpp"

#include <iostream>
#include <vector>

namespace foo {

  struct bar {
    gc<bar> derp;
    gc<int> data;

    ~bar() {
      std::clog << "bar dtor" << std::endl;
    }
  };

  void gc_mark(bar& self) {
    self.derp.mark();
    self.data.mark();    
  }
  
}



int main(int, char**) {

  // gc roots
  std::vector<gc_base> roots;
  
  auto collect = [&] {
    std::clog << "collect" << std::endl;

    for(auto& obj : roots) { obj.mark(); }
    object::sweep();
  };
  
  using namespace foo;
  gc<bar> ptr = make_gc<bar>();

  ptr->data = make_gc<int>(1);
  ptr->derp = ptr;              // cycle here

  gc<bar> ptr2 = make_gc<bar>();
  ptr2->derp = ptr;

  // push roots
  roots.push_back(ptr);
  roots.push_back(ptr2);
  
  collect();

  // pop roots & collect
  roots.pop_back();
  collect();

  roots.pop_back();
  collect();

  // empty root set
  collect();
  
  return 0;
}

