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
  std::size_t count;
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
  

  template<class Derived>
  struct tag {
    const std::size_t count;
    tag(std::size_t count) : count(count) { }
  };
  
  template<class Derived>
  void* operator new( std::size_t size, tag<Derived> tag ) {
    void* ptr = ::operator new(size + sizeof(T) * tag.count);
    
    // note: we need Derived here to upcast safely
    // note: volatile needed or gcc skips setting count
    volatile dynamic_sized* self = reinterpret_cast<Derived*>(ptr);
    self->count = tag.count;
    
    return ptr;
  }

  template<class Derived, class ... Args>
  static Derived* create(std::size_t count, Args&& ... args) {
    return new(tag<Derived>(count)) Derived(std::forward<Args>(args)...);
  }

  
  template<class Iterator>
  dynamic_sized(Iterator first, Iterator last) {
    assert( last - first == std::ptrdiff_t(count) );
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
  // TODO private?
  dynamic_sized() {
    for(T* it = data, *last = data + count; it != last; ++it) {
      new (it) T;
    }
  }

  
  dynamic_sized(const dynamic_sized&) = delete;
  
};



#endif
