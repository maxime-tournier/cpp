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


class derp {
protected:
  ieee754 data;
  using payload_type = ieee754::payload_type;

  void release() {
    payload_type tmp = data.bits.payload;
    reinterpret_cast<ref_any&>(tmp).~ref_any();
  }

public:
  
  template<class T>
  void set(const T& value) {
    static_assert( sizeof(T) <= sizeof(payload_type), "type too large");
    if(data.bits.nan && data.bits.flag) release();

    if(std::is_same<T, double>::value) {
      data.value = reinterpret_cast<const double&>(value);
    } else {
      data.bits.flag = false;
      data.bits.nan = ieee754::quiet_nan;
      data.bits.payload = reinterpret_cast<const ieee754::payload_type&>(value);
    }
  }

  void set(const ref_any& value) {
    if(!data.bits.nan || !data.bits.flag) {
      // turn the data into a null ptr if it wasnt a ptr
      data.bits.flag = true;
      data.bits.nan = ieee754::quiet_nan;
      data.bits.payload = 0;
    }

    payload_type tmp = data.bits.payload;
    reinterpret_cast<ref_any&>(tmp) = value;
    data.bits.payload = tmp;    
  }

  void set(ref_any&& value) {
    if(!data.bits.nan || !data.bits.flag) {
      // turn the data into a null ptr if it wasnt a ptr
      data.bits.flag = true;
      data.bits.nan = ieee754::quiet_nan;
      data.bits.payload = 0;
    }

    payload_type tmp = data.bits.payload;
    reinterpret_cast<ref_any&>(tmp) = std::move(value);
    data.bits.payload = tmp;
  }
  
  ~derp() {
    if(data.bits.nan && data.bits.flag) release();
  }

  
};





int main(int, char**) {

  derp x;
  x.set(1.0);

  ref<test> y = make_ref<test>();
  x.set(y.any());
  x.set( make_ref<test>().any() );
  return 0;
}
