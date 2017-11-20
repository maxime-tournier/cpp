// -*- compile-command: "c++ -g -Wall -std=c++11 nan_union.cpp -o nan_union -lstdc++" -*-

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


class nan_boxed {
protected:
  ieee754 data;
  using value_type = std::int64_t;
  
  void release() {
    value_type tmp = data.bits.payload;
    reinterpret_cast<ref_any&>(tmp) = {};
  }
  
public:

  ref_any as_ref() const {
    if(!data.bits.nan || !data.bits.flag) throw std::bad_cast();
    value_type tmp = data.bits.payload;
    return reinterpret_cast<ref_any&>(tmp);
  }

  const double& as_double() const {
    if(data.bits.nan) throw std::bad_cast();
    return data.value;
  }

  value_type as_value() const {
    if(!data.bits.nan || data.bits.flag) throw std::bad_cast();
    return data.bits.payload;
  }
  
  nan_boxed& operator=(const value_type& other) {
    if(data.bits.flag) release();
    data.bits.flag = false;
    data.bits.nan = ieee754::quiet_nan;
    data.bits.payload = other;
    return *this;
  }

  nan_boxed& operator=(const double& other) {
    if(data.bits.flag) release();
    data.value = other;
    return *this;
  }

  nan_boxed& operator=(const ref_any& other) {
    if(!data.bits.flag) {
      data.bits.flag = true;
      data.bits.nan = ieee754::quiet_nan;
    }

    value_type value;
    new (&value) ref_any(other);
    data.bits.payload = value;
    
    return *this;
  }

  nan_boxed& operator=(ref_any&& other) {
    if(!data.bits.flag) {
      data.bits.flag = true;
      data.bits.nan = ieee754::quiet_nan;
    }

    value_type value;
    new (&value) ref_any(std::move(other));
    data.bits.payload = value;
    
    return *this;
  }


  ~nan_boxed() {
    if(data.bits.flag) release();
  }

  nan_boxed(const nan_boxed& other) {
    if(other.data.bits.flag) {
      value_type value;

      // TODO optimize temporary
      new (&value) ref_any(other.as_ref());
      data.bits.payload = value;
      data.bits.flag = true;
      data.bits.nan = ieee754::quiet_nan;
    } else {
      data.value = other.data.value;
    }
  }


  nan_boxed(const ref_any& other) {
    value_type value;
    new (&value) ref_any(other);
    data.bits.flag = true;
    data.bits.nan = ieee754::quiet_nan;
    data.bits.payload = value;
  }

  nan_boxed(ref_any&& other) {
    value_type value;
    new (&value) ref_any(std::move(other));
    data.bits.flag = true;
    data.bits.nan = ieee754::quiet_nan;
    data.bits.payload = value;
  }
  
  
  
};





int main(int, char**) {
  nan_boxed x = make_ref<test>().any();

  // x = 1.0;
  std::clog << "finished" << std::endl;
  return 0;
}
