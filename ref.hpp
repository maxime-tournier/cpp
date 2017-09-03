#ifndef REF_HPP
#define REF_HPP

#include <utility>
#include <cassert>

namespace detail {
  
  struct rc_base {
    virtual ~rc_base() { }
    std::size_t rc = 0;

    friend void incref(rc_base* self) { self->rc++; }
    friend void decref(rc_base* self) {
      if(--self->rc == 0) delete self;
    }
  };


  // TODO wrap integral types?
  template<class T>
  struct control_block : rc_base, T {
    
    template<class ... Args>
    control_block(Args&& ... args) :
      T(std::forward<Args>(args)...) {
      
    }
    
  };


  // block_type must be derived from pointer_type, and implicitely convertible
  // to T
  template<class T, class = void >
  struct ref_traits {
    using block_type = control_block<T>;
  };


  // intrusive version
  template<class T>
  struct ref_traits< T, typename std::enable_if< std::is_base_of<rc_base, T>::value >::type > {
    using block_type = T;
  };
  
}

// a simple non-intrusive ref-counting pointer
template<class T>
class ref {
protected:

  using traits = detail::ref_traits<T>;
  using pointer_type = detail::rc_base; // typename traits::pointer_type;
  
private:
  pointer_type* ptr;
public:

  ref(pointer_type* ptr = nullptr) noexcept : ptr(ptr) {
    if(ptr) incref(ptr);
  }
  
  explicit operator bool() const { return ptr; }

  ~ref() { if(ptr) decref(ptr); }

  // copy
  ref(const ref& other) noexcept : ptr(other.ptr) {
    if(ptr) incref(ptr);
  }

  template<class Derived, decltype( std::declval<T*>() = std::declval<Derived*>() )* = 0>
  ref(const ref<Derived>& other) noexcept : ptr( other.template cast<T>().ptr) { }
  

  ref& operator=(const ref& other) noexcept {
    if(this == &other) return *this;
    
    if(ptr) {
      decref(ptr);
    }
    
    if((ptr = other.ptr)) {
      incref(ptr);
    }
    
    return *this;
  }


  // move
  ref(ref&& other) noexcept : ptr(other.ptr) {
    other.ptr = nullptr;
  }

  template<class Derived, decltype( std::declval<T*>() = std::declval<Derived*>() )* = 0>
  ref(ref<Derived>&& other) noexcept : ptr( other.template cast<T>().ptr) {
    other.template cast<T>().ptr = nullptr;
  }
  
  
  ref& operator=(ref&& other) noexcept {
    if(this == &other) return *this;
    
    if(ptr) {
      decref(ptr);
    }
    
    ptr = other.ptr;
    other.ptr = nullptr;
    return *this;
  }    

  
  template<class ... Args>
  static inline ref make(Args&& ... args) {
    using block_type = typename traits::block_type;
    
    pointer_type* ptr = new block_type(std::forward<Args>(args)...);
    return ref(ptr);
  }
  
  T* get() const {
    using block_type = typename traits::block_type;    
    return static_cast< block_type* >(ptr);
  }
  

  template<class Base>
  const ref<Base>& cast() const {
    T* derived = nullptr;
    Base* check = derived;
    (void) check;
    return reinterpret_cast< const ref<Base>& >(*this);
  }

  template<class Base>
  ref<Base>& cast() {
    T* derived = nullptr;
    Base* check = derived;
    (void) check;
    return reinterpret_cast<ref<Base>& >(*this);
  }

  
  bool operator==(const ref& other) const {
    return get() == other.get();
  }

  bool operator<(const ref& other) const {
    return get() < other.get();
  }

  
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }
  
};


template<class T, class ... Args>
static inline ref<T> make_ref(Args&& ... args) {
  return ref<T>::make(std::forward<Args>(args)...);
}



#endif
