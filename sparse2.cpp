
#include <memory>
#include <cassert>

#include <iostream>

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

    template<class Iterator>
    block(std::size_t mask, T** data, Iterator first, Iterator last):
      mask(mask), data(*data) {
      T* ptr = this->data;
      for(Iterator it = first; it != last; ++it, ++ptr) {
        new ((void*)ptr) T(*it);
      }
    }

    ~block() {
      for(const T& it: *this) { it.~T(); }
    }
  };

  std::shared_ptr<block> blk;

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


  template<class Iterator>
  static std::shared_ptr<block> make_block(std::size_t mask, Iterator first, Iterator last) {
    const std::size_t size = last - first;
    T* data;
    allocator<block> alloc{size, &data};
    std::shared_ptr<block> res = std::allocate_shared<block>(alloc, size, &data, first, last);
    return res;
  }

  
  template<class Iterator>
  array(std::size_t mask, Iterator first, Iterator last):
    blk(make_block(mask, first, last)) { }
};


int main() {

  double values[] = {2, 4};
  array<double> test(3, std::begin(values), std::end(values));
  std::clog << test.blk->data[0] << std::endl;
  std::clog << test.blk->data[1] << std::endl;
  return 0;
}
