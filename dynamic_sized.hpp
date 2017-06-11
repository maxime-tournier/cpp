#ifndef DYNAMIC_SIZED_HPP
#define DYNAMIC_SIZED_HPP

#include <stdexcept>
#include <cassert>

template<class T, class Derived>
class dynamic_sized {
  const std::size_t count;
  T data[0];

public:

  std::size_t size() const { return count; }
  
  T& operator[](std::size_t i) {
    assert(i < size);
    return data[i];
  }

  const T& operator[](std::size_t i) const {
    assert(i < size);
    return data[i];
  }
  
  void* operator new(std::size_t size, std::size_t count) {
    if(!count) throw std::bad_alloc();
    void* ptr = ::operator new(size + sizeof(T) * count);
    
    // note: we need Derived here to upcast safely
    dynamic_sized* self = reinterpret_cast<Derived*>(ptr);
    const_cast<std::size_t&>(self->count) = count;
    
    return ptr;
  }


  template<class Iterator>
  dynamic_sized(Iterator first, Iterator last) : count( size() ) {
    assert( last - first == size );
    std::size_t i = 0;
    for(Iterator it = first; it != last; ++it) {
      new (data + i++) T(*it);
    }
  };

  dynamic_sized() : count( size() ){
    for(std::size_t i = 0; i < count; ++i) {
      new (data + i) T;
    }
  }

  
  ~dynamic_sized() {
    for(std::size_t i = 0; i < count; ++i) {
      data[count - 1 - i].~T();
    }
    
  }

 
  
};



#endif
