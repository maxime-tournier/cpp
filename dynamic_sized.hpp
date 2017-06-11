#ifndef DYNAMIC_SIZED_HPP
#define DYNAMIC_SIZED_HPP

#include <stdexcept>
#include <cassert>

template<class T, class Derived>
class dynamic_sized {
  std::size_t size;
  T data[0];
public:

  T& operator[](std::size_t i) {
    assert(i < size);
    return data[i];
  }

  const T& operator[](std::size_t i) const {
    assert(i < size);
    return data[i];
  }
  
  void* operator new(std::size_t bytes, std::size_t size) {
    if(!size) throw std::bad_alloc();
    void* ptr = ::operator new(bytes + sizeof(T) * size);

    // note: we need Derived here to upcast safely
    dynamic_sized* self = reinterpret_cast<Derived*>(ptr);
    self->size = size;
    
    return ptr;
  }


  template<class Iterator>
  dynamic_sized(Iterator first, Iterator last) {
    assert( last - first == size );
    std::size_t i = 0;
    for(Iterator it = first; it != last; ++it) {
      new (data + i++) T(*it);
    }
  };
  
  ~dynamic_sized() {
    for(std::size_t i = 0; i < size; ++i) {
      data[size - 1 - i].~T();
    }
    
  }

 
  dynamic_sized() {
    for(std::size_t i = 0; i < size; ++i) {
      new (data + i) T;
    }
  }
  
};



#endif
