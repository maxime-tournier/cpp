#ifndef STACK_HPP
#define STACK_HPP

#include <vector>
#include <memory>
#include <stdexcept>
#include <cassert>

#include "slice.hpp"

class stack {
  std::vector<char> storage;
  std::size_t sp;

  // TODO not sure we need num here
  template<class G>
  std::size_t aligned_sp(std::size_t num) {
    std::size_t space = -1;
    void* buf = storage.data() + sp;
    char* aligned = (char*)std::align( alignof(G), num * sizeof(G), buf, space);
    return aligned - storage.data();
  }

public:

  struct frame {
    // stack pointer, buffer size, element count
    std::size_t sp, size, count;
  };
  
  struct overflow : std::runtime_error {
    stack& who;
    const std::size_t required;
    
    overflow(stack& who, std::size_t required)
      : std::runtime_error("stack overflow"),
        who(who),
        required(required) { }
    
  };
  
  
  stack(std::size_t size = 0)
    : storage(size),
      sp(0) {
    
  }
  
  template<class G>
  slice<G> allocate(frame& f, std::size_t count) {
    assert(count > 0 && "zero-size allocation");
    
    const std::size_t size = count * sizeof(G);

    if(!storage.capacity()) {
      reserve(size);
    }
    
    // align
    sp = aligned_sp<G>(count);
    const std::size_t required = sp + size;
    
    if( required > storage.capacity() ) {
      throw overflow(*this, required);
    }
    
    // allocate space
    storage.resize( required );

    G* res = reinterpret_cast<G*>(&storage[sp]);
    
    // record frame
    f = {sp, size, count};
    sp += size;
	
    return {res, res + count};
  }


  template<class G>
  slice<G> get(const frame& f) {
	assert(f.count);
    G* first = reinterpret_cast<G*>(&storage[f.sp]);
	return {first, first + f.count};
  }

  template<class G>
  slice<const G> get(const frame& f) const {
	assert(f.count);	
	const G* first = reinterpret_cast<const G*>(&storage[f.sp]);
	return {first, first + f.count};
  }

  void reset() {
    sp = 0;
  }

  std::size_t capacity() const { return storage.capacity(); }
  std::size_t size() const { return storage.size(); }

  void reserve(std::size_t size) {
    storage.reserve(size);
  }

  void grow(std::size_t required = 0) {
    const std::size_t c = capacity();
    assert( c );

    const std::size_t min = std::max(required, c+1);
    reserve( std::max(min, c + c / 2) );
  }

  slice<const char> peek() const {
    return {storage.data(), storage.data() + size() };
  }

  slice<char> peek() {
    return {storage.data(), storage.data() + size() };
  }
  
};


#endif
