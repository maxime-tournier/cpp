#ifndef SPARSE_SPARSE_HPP
#define SPARSE_SPARSE_HPP

#include <memory>
#include <cassert>

namespace sparse {
  
  static std::size_t sparse_index(std::size_t mask, std::size_t index) {
    const std::size_t res = __builtin_popcountl(mask & ((1ul << index) - 1));      
    return res;
  }

  static std::size_t sparse_first(std::size_t mask) {
    const std::size_t res = __builtin_ctzl(mask);
    return res;
  }


  static std::size_t sparse_size(std::size_t mask) {
    return __builtin_popcountl(mask);
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
      
      assert((!insert) <= (sparse_index(mask, index) < (last - source)));
      assert(insert <= (sparse_index(mask, index) == (last - source)));      
      
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
    auto&& result = std::allocate_shared<block>(alloc, &data, std::forward<Args>(args)...);
    assert(sparse_size(result->mask) == size);
    
    return result;
  }

  array() { }
  array(std::shared_ptr<block> blk): blk(std::move(blk)) { }

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
    return blk->mask & (1ul << index);
  }

  array set(std::size_t index, const T& value) const {
    if(!blk) {
      return make_block(1ul, index, value);
    }

    const std::size_t bit = 1ul << index;
    
    const bool insert = !(blk->mask & bit);
    
    assert(!insert <= (sparse_index(blk->mask, index) < size()));
    assert(insert <= (sparse_index(blk->mask | bit, index) == size()));
    
    const std::size_t s = insert ? size() + 1 : size();
    
    return make_block(s, (blk->mask | bit), blk->begin(), blk->end(), index, value, insert);
  }

  bool unique() const { return blk.unique(); }

  template<class Cont>
  void iter(Cont cont) const {
    if(!blk) return;

    std::size_t off = 0;
    std::size_t mask = blk->mask;
    for(const T& value: *blk) {
      const std::size_t fst = sparse_first(mask);
      off += fst;
      mask >>= fst;
      
      cont(off, value);
      
      mask >>= 1;
      off += 1;
    }
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
