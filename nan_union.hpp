#ifndef CPP_NAN_UNION_HPP
#define CPP_NAN_UNION_HPP

#include <bitset>

// pick the right index/constructor from a value
template<std::size_t size, class ... T> struct selector;

template<std::size_t size> struct selector<size> {
  static void index() { }
  static void construct() { }    
};
  
template<std::size_t size, class H, class ... T>
struct selector<size, H, T...> : selector<size, T...> {

  using selector<size, T...>::index;
  static constexpr std::integral_constant<std::size_t, 
                                          size - (1 + sizeof...(T))> 
  index(const H& ) {
    return {};
  }
};


// a nan-tagged ieee754 double
union ieee754 {
  double value;
  using payload_type = std::uint64_t;
  struct bits_type {
    payload_type payload : 48;
    unsigned char type : 3;
    unsigned int nan : 12;
    bool flag : 1;
  } bits;

  static const unsigned int quiet_nan = (1 << 12) - 1;
  ieee754() { }

  std::bitset<64> debug;
};


template<class ... T>
class nan_union {
  ieee754 data;
  using selector_type = selector<sizeof...(T), T...>;
  using payload_type = ieee754::payload_type;  
public:

  template<class U, unsigned index = decltype( selector_type::index( std::declval<U>() ) )::value>
  static constexpr unsigned type_index() { return index; }
  
  template<class U> static void destruct(payload_type self) {
    reinterpret_cast<U&>(self).~U();
  };

  void release() {
    using thunk_type = void (*) (payload_type);    
    static const thunk_type thunk[] = { destruct<T>... };
    thunk[data.bits.type](data.bits.payload);
  }

  template<class U, class ... Args, unsigned type = type_index<U>()>
  void construct(Args&& ... args) {

    using value_type = typename std::decay<U>::type;
    
    data.bits.nan = ieee754::quiet_nan;
    data.bits.type = type;
    data.bits.flag = !std::is_trivially_destructible< value_type >();
    
    payload_type tmp;
    new (&tmp) value_type(std::forward<Args>(args)...);
    data.bits.payload = tmp;
  }
  
  template<class U>
  static void copy_construct(nan_union& self, const nan_union& other) {
    self.construct<U>(other.template get<U>());
  }

  template<class U>
  static void move_construct(nan_union& self, nan_union&& other) {
    self.construct<U>(std::move(other.template get<U>()));
  }
  
public:
  nan_union(const double& value) noexcept { data.value = value; }
  
  template<class U, unsigned R = type_index<U>() >
  nan_union(U&& value) { construct<U>( std::forward<U>(value)); }
  
  template<class U, int type = type_index<U>() >
  nan_union& operator=(U&& x) noexcept {
    const bool same_type = data.bits.type == type;

    // need to release if assigning from different types and release
    // bit is set
    if(data.bits.nan && same_type && data.bits.flag) {
      release();
    }

    if(same_type) {
      payload_type tmp;
      reinterpret_cast<U&>(tmp) = std::forward<U>(x);
      data.bits.payload = tmp;
    } else {
      construct<U>(std::forward<U>(x));
    }

    return *this;
  }
  

  nan_union& operator=(const double& x) noexcept {
    if(data.bits.nan && data.bits.flag) release();
    data.value = x;
    return *this;
  }


  ~nan_union() noexcept {
    if(data.bits.nan && data.bits.flag) release();
  }
  
  nan_union(const nan_union& other) noexcept {
    static constexpr bool fast[] = { std::is_trivially_copy_constructible<T>::value... };
    
    if(!other.data.bits.nan || fast[other.data.bits.type]) {
      data.value = other.data.value;
      return;
    }

    using thunk_type = void (*) (nan_union&, const nan_union& other);
    static const thunk_type thunk[] = { copy_construct<T>... };
    thunk[other.data.bits.type](*this, other);
  }

  nan_union(nan_union&& other) noexcept {
    static constexpr bool fast[] = { std::is_trivially_move_constructible<T>::value... };
    
    if(!other.data.bits.nan || fast[other.data.bits.type]) {
      data.value = other.data.value;
      return;
    }

    using thunk_type = void (*) (nan_union&, nan_union&& other);
    static const thunk_type thunk[] = { move_construct<T>... };
    thunk[other.data.bits.type](*this, std::move(other));
  }

  template<class U, int type = type_index<U>() >
  U get() const {
    if( std::is_same<U, double>::value && !data.bits.nan ) {
      return reinterpret_cast<const U&>(data.value);
    }
    if( data.bits.nan && data.bits.type == type ) {
      const payload_type payload = data.bits.payload;
      return reinterpret_cast<const U&>(payload);
    }

    throw std::bad_cast();
  }
  
};




#endif
