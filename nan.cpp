#ifndef CPP_NAN_HPP
#define CPP_NAN_HPP

#include <cstdint>
#include <cassert>

#include <iostream>


union ieee754 {
  double value;
  std::uint64_t bits;
};

// basic nan-tagged storage facility
class nan_storage {
  ieee754 storage;

  // nan bits mask: all 11 exponent bits are set, which leaves us with 52
  // mantissa bits + sign bit for storing stuff
  static constexpr std::uint64_t nan_mask = ((std::uint64_t(1) << 11) - 1) << 52;

  // quiet bit mask: actually, only 51 mantissa bits since bit 51 signals a
  // 'quiet nan'. that's a total 52 bits for storing information.
  static constexpr std::uint64_t quiet_mask = std::uint64_t(1) << 51;

  // quiet nan bits mask
  static constexpr std::uint64_t quiet_nan_mask = nan_mask | quiet_mask;

  // sign bit mask
  static constexpr std::uint64_t sign_mask = std::uint64_t(1) << 63;

  // remaining data mask
  static constexpr std::uint64_t data_mask = (std::uint64_t(1) << 51) - 1;
public:

  // test/access double
  bool is_double() const { return (storage.bits & quiet_nan_mask) != quiet_nan_mask; }
  
  double& as_double() { return storage.value; }
  const double& as_double() const { return storage.value; }  
  
  // get/set sign bit
  bool sign() const { return storage.bits & sign_mask; }
  
  void sign(bool value) {
    if(value) storage.bits |= sign_mask;
    else storage.bits &= ~sign_mask;
  }

  // get/set data chunk
  std::uint64_t data() const {
    return storage.bits & data_mask;
  }
  
  void data(std::uint64_t x) {
    assert( x <= data_mask );
    // clear data, keeping nan mask
    storage.bits &= quiet_nan_mask | ~data_mask;
    storage.bits |= (data_mask & x);
  }  


  nan_storage() { }
  
  nan_storage(double value) {
    storage.value = value;
  }
  
  nan_storage(bool sign, std::uint64_t data) {
    storage.bits = quiet_nan_mask;
    this->sign(sign);
    this->data(data);
  }


  friend std::ostream& operator<<(std::ostream& out, const nan_storage& self) {
    if(self.is_double()) {
      return out << self.as_double();
    } else {
      return out << '(' << self.sign() << " " << std::hex << self.data() << ')';
    }
  }

  
};


class nan_union {
  nan_storage storage;

  // hide tag in 3 bits 48-50 as pointers only use 48bits addressing space
  static constexpr std::uint64_t tag_shift = 48;
  static constexpr std::uint64_t tag_width = 3;
  static constexpr std::uint64_t tag_max = (std::uint64_t(1) << tag_width) - 1;
  static constexpr std::uint64_t tag_mask = tag_max << tag_shift;

  // the remaining 48 bits hold the value
  static constexpr std::uint64_t value_mask = (std::uint64_t(1) << tag_shift ) - 1;
  
public:

  using tag_type = unsigned char;
  using value_type = std::uint64_t;

  // double
  bool is_double() const { return storage.is_double(); }
  double& as_double() { return storage.as_double(); }
  const double& as_double() const { return storage.as_double(); }    
  
  // get/set tag
  tag_type tag() const { return storage.data() & tag_mask; }
  void tag(tag_type x) { storage.data( value() | (value_type(x) << tag_shift) ); }
  

  // get/set value
  value_type value() const { return storage.data() & value_mask; }

  void value(value_type x) {
    assert( (~value_mask & x) == 0 );
    return storage.data( x | value_type(tag()) << tag_shift );
  }

  nan_union()  { }
  nan_union(const double& x) : storage(x) { }

  nan_union(tag_type tag, value_type value)
    : storage( false, value | (value_type(tag) << tag_shift) ) {
    assert( value <= value_mask );
    assert( tag <= tag_max );
  }


  friend std::ostream& operator<<(std::ostream& out, const nan_union& self) {
    if(self.is_double()) {
      return out << self.as_double();
    } else {
      out << "\t" << self.storage << std::endl;
      return out << '(' << int(self.tag()) << " " << std::hex << self.value() << ')' << std::endl;
    }
  }
  
};




int main(int, char**) {

  // low-level
  nan_storage x1 = 1.0;

  auto* ptr = new int;
  nan_storage x2(true, (std::uint64_t) ptr);

  std::clog << "x1: " << x1 << std::endl;
  std::clog << "x2: " << x2 << std::endl;  
  std::clog << "ptr: " << ptr << std::endl;


  // middle level
  nan_union u1 = 10.0;
  nan_union u2(7, (std::uint64_t) ptr);

  std::clog << u1 << std::endl;
  std::clog << u2 << std::endl;  
  
  // std::clog << "test: " << std::hex << (std::uint64_t(1) << 12) - 1 << std::endl;
  // std::clog << std::hex << nan_mask1 << " " << nan_mask2 << std::endl;
};



#endif
