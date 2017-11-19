#ifndef DYNAMIC_SIZED_HPP
#define DYNAMIC_SIZED_HPP

#include <stdexcept>
#include <cassert>



// rules:

// 1. don't allocate on the stack, always use new

// 2. don't use as class member

// 3. don't derive from if the derived class has data members, put the members
// in another class and derive from it before

// 4. use `create` to create derived instances unless you know what you're doing

template<class T>
class dynamic_sized {
  const std::size_t count;
  T data[0];
public:
  std::size_t size() const { return count; }
  
  T& operator[](std::size_t i) {
    assert(i < count);
    return data[i];
  }

  const T& operator[](std::size_t i) const {
    assert(i < count);
    return data[i];
  }

  T* begin() { return data; }
  T* end() { return data + size(); }  

  const T* begin() const { return data; }
  const T* end() const { return data + size(); }  

  void* operator new( std::size_t size, std::size_t count ) {
    return ::operator new(size + sizeof(T) * count);
  }
  
  template<class Iterator>
  dynamic_sized(Iterator first, Iterator last)
    : count(last - first) {
    std::size_t i = 0;
    for(Iterator it = first; it != last; ++it) {
      new (data + i++) T(*it);
    }
  };
  
  ~dynamic_sized() {
    for(T* it = data + count - 1, *last = data; it >= last; --it) {
      it->~T();
    }
  }

protected:
  dynamic_sized() = delete;
  dynamic_sized(const dynamic_sized&) = delete;
  
};



#endif
