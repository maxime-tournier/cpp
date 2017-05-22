#ifndef REF_HPP
#define REF_HPP

#include <utility>

// a simple non-intrusive ref-counting pointer
template<class T>
class ref {
  
  struct block_type {

    T value;
    std::size_t rc;    

    
    template<class ... Args>
    block_type(Args&& ... args)
      : // rc(1),
      value( std::forward<Args>(args) ... )
      , rc(1)
    {
    }
  };

  block_type* block;

  ref(block_type* block) : block(block) { }

public:

  explicit operator bool() const { return block; }

  // default
  ref() noexcept : block(nullptr) { }

  ~ref() noexcept {
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
    if(block && (--block->rc == 0)) {
      delete block;
    }
    
    if((block = other.block)) ++block->rc;
    return *this;
  }


  // move
  ref(ref&& other) noexcept : block(other.block) {
    other.block = nullptr;
  }

  ref& operator=(ref&& other) noexcept {
    block = other.block;
    other.block = nullptr;
    return *this;
  }    
  
  template<class ... Args>
  static inline ref make(Args&& ... args) {
    return new block_type(std::forward<Args>(args)...);
  }


  bool operator==(const ref& other) const { return block == other.block; }
  
  T* get() const {
    return // reinterpret_cast<T*>(block);
      &block->value;
  }

  T* operator->() const {
    return // reinterpret_cast<T*>(block);
      &block->value;
  }


  T& operator*() const {
    return block->value;
  }
  
  
};


template<class T, class ... Args>
static inline ref<T> make_ref(Args&& ... args) {
  return ref<T>::make(std::forward<Args>(args)...);
}



#endif
