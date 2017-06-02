#ifndef GC_HPP
#define GC_HPP

#include <utility>


class object {
  static object* start = nullptr;
  
  // we hide the mark in the low order bits of next
  using mask_type = std::uintptr_t;
  static constexpr mask_type mask = (-1) << 1;

  // unmarked next is the actual pointer
  object* next;
  
public:

  inline void mark() { next |= ~mask; }
  inline void clear() { next &= mask; }
  inline bool marked() const { return rc & ~mask; }

  inline object() : next(start) { start = this; }
  
  virtual ~object() { }
  
  static void sweep() {
    object** obj = &start;

    while(*obj) {
      if( (*obj)->marked() ) {
        // clear & move on
        (*obj)->clear();
        obj = &(*obj)->next;
        
      } else {
        // plug ends and delete
        object* to_delete = *obj;
        *obj = to_delete->next;
        
        delete to_delete;
      } 
    }
  }
  
};

// a simple mark & sweep gc pointer
template<class T>
class gc {
  
  struct block_type : object {
    T value;

    template<class ... Args>
    block_type(Args&& ... args)
      : value( std::forward<Args>(args) ... ) {
      
    }
  };

  block_type* block = nullptr;
  gc(block_type* block) : block(block) { }
  
public:
  
  explicit operator bool() const { return block; }
  
  gc() noexcept { }
  
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
