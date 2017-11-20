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
  using tag_type = ieee754::tag_type;
  
  void release() {
    value_type tmp = data.bits.payload;
    reinterpret_cast<ref_any&>(tmp) = {};
  }
  
public:  
  
  ieee754::tag_type tag() const {
    if(!data.bits.nan) throw std::bad_cast();
    return data.bits.tag;
  }


  void tag(ieee754::tag_type tag) {
    if(!data.bits.nan) throw std::bad_cast();
    data.bits.tag = tag;
  }
  
  bool is_ref() const { return data.bits.nan && data.bits.flag; }
  bool is_double() const { return !data.bits.nan; }
  bool is_value() const { return data.bits.nan && !data.bits.flag; }    
  
  template<class F>
  typename F::value_type visit(const F& f) const {
    if(!data.bits.nan) {
      return f(data.value);
    } else if( data.bits.flag ) {
      const value_type tmp = data.bits.payload;
      return f(reinterpret_cast<const ref_any&>(tmp));
    } else {
      const value_type tmp = data.bits.payload;
      return f(tmp);
    }
  }


  ~nan_boxed() {
    if(data.bits.flag) release();
  }

  nan_boxed(const nan_boxed& other) {
    if(other.data.bits.flag) {
      value_type dst, src;
      
      src = other.data.bits.payload;
      new (&dst) ref_any(reinterpret_cast<ref_any&>(src));
      
      data.bits.payload = dst;
      data.bits.flag = true;
      
      data.bits.tag = other.data.bits.tag;
      data.bits.nan = ieee754::quiet_nan;
    } else {
      data.value = other.data.value;
    }
  }


  nan_boxed(nan_boxed&& other) {
    if(other.data.bits.flag) {
      value_type dst, src;
      
      src = other.data.bits.payload;
      new (&dst) ref_any( std::move(reinterpret_cast<ref_any&>(src)));
      other.data.bits.payload = src;
      
      data.bits.flag = true;
      data.bits.nan = ieee754::quiet_nan;      
      data.bits.tag = other.data.bits.tag;      
      data.bits.payload = dst;
      
    } else {
      data.value = other.data.value;
    }
  }


  
  nan_boxed(const ref_any& other, const tag_type& tag) {
    value_type value;
    new (&value) ref_any(other);
    data.bits.flag = true;
    data.bits.nan = ieee754::quiet_nan;
    data.bits.payload = value;
  }

  nan_boxed(ref_any&& other, const tag_type& tag) {
    value_type value;
    new (&value) ref_any(std::move(other));
    data.bits.flag = true;
    data.bits.nan = ieee754::quiet_nan;
    data.bits.tag = tag;
    data.bits.payload = value;
  }
  
  nan_boxed(const double& other) {
    data.value = other;
  }

  nan_boxed(const value_type& value, const tag_type& tag) {
    data.bits.flag = false;
    data.bits.nan = ieee754::quiet_nan;
    data.bits.tag = tag;
    data.bits.payload = value;
  }
  
};





int main(int, char**) {
  nan_boxed x = {make_ref<test>().any(), 0};

  nan_boxed y = std::move(x);

  // x = 1.0;
  std::clog << "finished" << std::endl;
  return 0;
}
