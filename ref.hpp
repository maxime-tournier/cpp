#ifndef CPP_REF_HPP
#define CPP_REF_HPP

#include <stdexcept>
#include <utility>
#include <cassert>


struct ref_counted {
  virtual ~ref_counted() { }
  std::size_t rc = 0;
    
  friend void incref(ref_counted* self) { self->rc++; }
  friend void decref(ref_counted* self) { if(--self->rc == 0) delete self; }
};

template<class T> class ref;

class ref_any {
  using ptr_type = ref_counted*;
  ptr_type ptr;
public:
  ref_any() : ptr(nullptr) { }
  
  ref_any(ptr_type ptr) : ptr(ptr) {
    if(ptr) incref(ptr);
  }
    
  ptr_type get() const { return ptr; }
    
  ~ref_any() { if(ptr) decref(ptr); }

  ref_any(const ref_any& other) noexcept : ptr(other.ptr) {
    if(ptr) incref(ptr);
  }

  ref_any(ref_any&& other) noexcept : ptr(std::move(other.ptr)) {
    other.ptr = nullptr;
  }
    
  ref_any& operator=(const ref_any& other) noexcept {
    if(this == &other) return *this;

    if(ptr) decref(ptr);
    ptr = other.ptr;
    if(ptr) incref(ptr);
      
    return *this;
  }

  ref_any& operator=(ref_any&& other) noexcept{
    if(this == &other) return *this;

    if(ptr) decref(ptr);
    ptr = std::move(other.ptr);
    other.ptr = nullptr;
      
    return *this;
  }

  explicit operator bool() const { return ptr; }
  bool operator==(const ref_any& other) const { return ptr == other.ptr; }
  bool operator!=(const ref_any& other) const { return ptr != other.ptr; }
  bool operator<(const ref_any& other) const { return ptr < other.ptr; }

  std::size_t rc() const {
    if(ptr) return ptr->rc;
    throw std::invalid_argument("null pointer");
  }
    
  // type recovery
  template<class T> ref<T> cast() const;
};
  

template<class T>
struct control_block : ref_counted, T {
  template<class ... Args>
  control_block(Args&& ... args) : T(std::forward<Args>(args)...) { }
};


template<class T>
class ref {
  ref_any impl;
protected:
  ref(control_block<T>* block) : impl(block) { }
  friend ref ref_any::cast<T>() const;
public:
    
  template<class ... Args>
  static ref make(Args&& ... args) {
    return new control_block<T>(std::forward<Args>(args)...);
  }
    
  T* get() const {
    if(impl) return static_cast< control_block<T>* >(impl.get());
    return nullptr;
  }

  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }

  ref() = default;
  ref(const ref& ) = default;
  ref(ref&& ) = default;

  ref& operator=(const ref& ) = default;
  ref& operator=(ref&& ) = default;

  // type erasure
  ref_any any() const { return impl; }

  explicit operator bool() const { return bool(impl); }
  bool operator==(const ref& other) const { return impl == other.impl; }
  bool operator!=(const ref& other) const { return impl != other.impl; }
  bool operator<(const ref& other) const { return impl < other.impl; }
  
};

template<class T>
ref<T> ref_any::cast() const {
  return static_cast<control_block<T>*>(ptr);
}

template<class T, class ... Args>
static ref<T> make_ref(Args&& ... args) { return ref<T>::make(std::forward<Args>(args)...); }
  


#endif
