// -*- compile-command: "c++ -g -Wall -std=c++11 nan_union.cpp -o nan_union" -*-

#include "nan_union.hpp"
#include "ref.hpp"

#include <iostream>


struct test {
  test() { std::clog << "test()" << std::endl; }
  test(const test&) { std::clog << "test(const test&)" << std::endl; }
  test(test&&) { std::clog << "test(test&&)" << std::endl; }

  test& operator=(const test&) { std::clog << "operator=(const test&)" << std::endl; return *this;}        
  test& operator=(test&&) { std::clog << "operator=(test&&)" << std::endl; return *this;}

  ~test() {
    std::clog << "~test()" << std::endl;
  }

  friend std::ostream& operator<<(std::ostream& out, const test& self) {
    return out << "test!";
  }
  
};






int main(int, char**) {
  // nan_union<test, std::int32_t, ref<test> > u = test();
  // u = make_ref<test>();
  
  // std::clog << u.get<ref<test>>().get() << std::endl;
  // // u = 3.0;

  ref<test> x = make_ref<test>();
  ref<test> y = std::move(x);

  ref_any z = y.any();
  ref<test> w = z.cast<test>();
  std::clog << z.rc() << std::endl;
  
  return 0;
}
