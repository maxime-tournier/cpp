#ifndef REF_HPP
#define REF_HPP

#include <utility>
#include <cassert>

namespace detail {
  
  struct control_block_base {
    virtual ~control_block_base() { }
    std::size_t rc = 1;
  };


  // TODO wrap integral types
  template<class T>
  struct control_block : control_block_base, T {
    
    template<class ... Args>
    control_block(Args&& ... args) :
      T(std::forward<Args>(args)...) {
      
    }
    
  };
}

// a simple non-intrusive ref-counting pointer
template<class T>
class ref {
protected:
  using block_type = detail::control_block_base;  
  ref(block_type* block) : block(block) { }

private:
  block_type* block;
public:
  
  explicit operator bool() const { return block; }

  // default
  ref() noexcept : block(nullptr) { }

  ~ref() {
    assert(!block || block->rc );
    if(block && --block->rc == 0) {
      delete block;
    }
    
  }

  // copy
  ref(const ref& other) noexcept : block(other.block) {
    if(block) ++block->rc;
  }


  ref& operator=(const ref& other) noexcept {
    if(this == &other) return *this;
    
    if(block && (--block->rc == 0)) {
      delete block;
    }

    if((block = other.block)) {
      ++block->rc;
    }
    
    return *this;
  }


  // move
  ref(ref&& other) noexcept : block(other.block) {
    other.block = nullptr;
  }

  template<class Derived>
  ref(ref<Derived>&& other) noexcept : block( other.template cast<T>().block) {
    other.template cast<T>().block = nullptr;
  }
  
  
  ref& operator=(ref&& other) noexcept {
    if(this == &other) return *this;
    
    if(block && --block->rc == 0) {
      delete block;
    }
    
    block = other.block;
    other.block = nullptr;
    return *this;
  }    

  
  template<class ... Args>
  static inline ref make(Args&& ... args) {
    return new detail::control_block<T>(std::forward<Args>(args)...);
  }

  T* get() const {
    return static_cast<detail::control_block<T>*>(block);
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
    return block == other.block;
  }

  bool operator<(const ref& other) const {
    return block < other.block;
  }

  
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }
  
};


template<class T, class ... Args>
static inline ref<T> make_ref(Args&& ... args) {
  return ref<T>::make(std::forward<Args>(args)...);
}



#endif
