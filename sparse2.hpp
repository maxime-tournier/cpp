#ifndef SPARSE2_HPP
#define SPARSE2_HPP


#include <memory>
#include <cassert>

#include <iostream>

namespace sparse { 
static std::size_t sparse_index(std::size_t mask, std::size_t index) {
  const std::size_t res = __builtin_popcount(mask & ((1ul << index) - 1));      
  return res;
}

static std::size_t sparse_size(std::size_t mask) {
  return __builtin_popcount(mask);
}




template<class T>
struct array {

  struct block {
    const std::size_t mask;
    T* const data;

    const T* begin() const { return data; }
    const T* end() const { return data + sparse_size(mask); }
    
    block(const block&) = delete;

    // add element
    block(T** data, std::size_t mask, const T* source, const T* last,
          std::size_t index, const T& value,
          bool insert):
      mask(mask), data(*data) {
      
      T* ptr = *data;
      for(std::size_t i = 0, n = sparse_index(mask, index); i < n; ++i) {
        new (ptr++) T(*source++);
      }

      // init value at correct index
      new (ptr++) T(value);
      if(!insert) ++source;     // skip source value if not inserting

      while(source != last) {
        new (ptr++) T(*source++);
      }
    }

    // single element
    block(T** data, std::size_t index, const T& value):
      mask(1ul << index),
      data(*data) {
      new(*data) T(value);
    }
    
    ~block() {
      for(const T& it: *this) { it.~T(); }
    }
  };

  std::shared_ptr<block> blk;

  template<class ... Args>
  static std::shared_ptr<block> make_block(std::size_t size, Args&& ... args) {
    T* data;
    allocator<block> alloc{size, &data};
    return std::allocate_shared<block>(alloc, &data, std::forward<Args>(args)...);
  }

  array() { }
  array(std::shared_ptr<block> blk): blk(std::move(blk)) { }

  // array(array&&) = default;
  // array(const array&) = default;

  // array& operator=(const array&) = default;
  // array& operator=(array&&) = default;    
  
  const T& get(std::size_t index) const {
    return blk->data[sparse_index(blk->mask, index)];
  }

  T& get(std::size_t index) {
    return blk->data[sparse_index(blk->mask, index)];
  }
  
  std::size_t size() const {
    if(!blk) return 0;
    return sparse_size(blk->mask);
  }

  bool contains(std::size_t index) const {
    if(!blk) return false;
    return blk->mask & 1ul << index;
  }

  array set(std::size_t index, const T& value) const {
    if(!blk) {
      return make_block(1, index, value);
    }

    const std::size_t bit = 1ul << index;
    const bool insert = !(blk->mask & bit);
    const std::size_t s = insert ? size() + 1 : size();
    return make_block(s, blk->mask | bit, blk->begin(), blk->end(), index, value, insert);
  }
  
  
private:

  template<class V>
  struct allocator {
    using value_type = V;
      
    template<class U>
    struct rebind {
      using other = allocator<U>;
    };
      
    const std::size_t size;
    T** const data;

    template<class U>
    allocator(allocator<U> other): size(other.size), data(other.data) { }

    allocator(std::size_t size, T** data): size(size), data(data) { }

    V* allocate(std::size_t n) {
      assert(n == 1);
      
      struct layout {
        V head;
        T tail[0];
      };

      auto mem = (layout*)std::malloc(sizeof(layout) + size * sizeof(T));
      *data = mem->tail;
      return &mem->head;
    }

    void deallocate(void* p, std::size_t n) {
      std::free(p);
    }
    
  };
  
};

}


#endif
