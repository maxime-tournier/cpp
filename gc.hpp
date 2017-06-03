#ifndef GC_HPP
#define GC_HPP

#include <utility>
#include <cstdint>


// a simple mark & sweep object base
class object {
  static object* start;
  
  // we hide the mark in the low order bits of next
  using mask_type = std::uintptr_t;
  static constexpr mask_type mask = mask_type(-1) << 1;

  // unmarked next is the actual pointer
  object* next;
  
public:

  inline void mark() { reinterpret_cast<mask_type&>(next) |= ~mask; }
  inline void clear() { reinterpret_cast<mask_type&>(next) &= mask; }
  
  inline bool marked() const { return mask_type(next) & ~mask; }

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
        // plug hole & delete
        object* to_delete = *obj;
        *obj = to_delete->next;
        
        delete to_delete;
      } 
    }
  }

  virtual void traverse() = 0;
};

object* object::start = nullptr;

template<class T>
static inline void gc_mark(T& ) { }

template<class T> class gc;

// gc pointer base
class gc_base {
protected:
  object* obj;
  gc_base(object* obj) : obj(obj) { }
  
public:
  explicit operator bool() const { return obj; }
  bool operator==(const gc_base& other) const { return obj == other.obj; }

  gc_base() : obj(nullptr) { }

  void mark() const {
    if(!obj || obj->marked()) return;
    obj->mark();
    obj->traverse();
  }


  template<class T>
  gc<T> cast() const {
    return reinterpret_cast<const gc<T>&>(*this);
  }
  
};

// typed gc pointer: client types may customize gc_mark(T&) to mark sub gc
// pointer objects
template<class T>
class gc : public gc_base {
  
  struct block_type : object {
    T value;

    template<class ... Args>
    block_type(Args&& ... args) : value( std::forward<Args>(args) ... ) {  }

    void traverse() { gc_mark(value); }
    
  };

  gc(block_type* block) : gc_base(block) { }

   block_type* block() const {
    return static_cast<block_type*>(obj);
  }
  
public:

  gc() {}
  
  T* get() const {
    if(!block()) return nullptr;
    return &block()->value;
  }

  
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }
  
  template<class ... Args>
  static inline gc make(Args&& ... args) {
    return new block_type(std::forward<Args>(args)...);
  }

};


template<class T, class ... Args>
static inline gc<T> make_gc(Args&& ... args) {
  return gc<T>::make(std::forward<Args>(args)...);
}





#endif
