#ifndef ALT_HPP
#define ALT_HPP

#include <memory>
#include <cassert>
#include <array>

namespace alt { 

  namespace sparse {

    static std::size_t size(std::size_t mask) {
      return __builtin_popcountl(mask);
    }
  
    static std::size_t index(std::size_t mask, std::size_t index) {
      return size(mask & ((1ul << index) - 1));      
    }
  
  }

  template<class T, class V>
  struct allocator {
    using value_type = V;
    
    template<class U>
    struct rebind {
      using other = allocator<T, U>;
    };

    static constexpr std::size_t max_size = 1;
    
    const std::size_t size;
    T** const data;

    template<class U>
    allocator(allocator<T, U> other): size(other.size), data(other.data) { }
    
    allocator(std::size_t size, T** data):
      size(size), data(data) {
      assert(size);
    }

    V* allocate(std::size_t n) {
      struct layout {
        V head;
        T tail;
      };

      auto mem = (layout*)std::malloc(sizeof(layout) + (size - 1) * sizeof(T));
      *data = &mem->tail;
      return &mem->head;
    }

    void deallocate(void* p, std::size_t n) {
      std::free(p);
    }
    
  };

  

  template<class T>
  struct array {
    const std::size_t mask;
    T* const data;
    
    array(std::size_t mask, T** allocated):
      mask(mask), data(*allocated) {
      for(std::size_t i = 0, n = size(); i < n; ++i) {
        new (data + i) T;
      }
    }

    array(): mask(0), data(nullptr) { }
    
    ~array() {
      for(std::size_t i = 0, n = size(); i < n; ++i) {
        data[i].~T();
      }
    }
    
    std::size_t size() const { return sparse::size(mask); }
    T& at(std::size_t index) { return data[sparse::index(mask, index)]; }

    std::shared_ptr<array> set(std::size_t index, T&& value) const;

    bool has(std::size_t index) const { return mask & (1ul << index); }
  };



  template<class T>
  static std::shared_ptr<array<T>> make_array(std::size_t mask) {
    T* data;
    return std::allocate_shared<array<T>>(allocator<T, array<T>>{sparse::size(mask), &data},
                                          mask, &data);
  };



  template<class T>
  std::shared_ptr<array<T>> array<T>::set(std::size_t index, T&& value) const {
    const T* in = data;
    const std::size_t bit = 1ul << index;

    auto res = make_array<T>(mask | bit);

    const std::size_t s = sparse::index(res->mask, index);
    T* out = res->data;
  
    // copy leading values
    for(std::size_t i = 0; i < s; ++i) {
      *out++ = *in++;
    }

    // copy set value
    res->data[s] = std::move(value);

    // skip corresponding input if bit was set
    if(const bool skip = mask & bit) {
      ++in;
    }

    // copy trailing values
    for(std::size_t i = s + 1, n = res->size(); i < n; ++i) {
      *out++ = *in++;
    }

    return res;
  }

  struct base {
    using storage_type = std::shared_ptr<void>;
    storage_type storage;

    base(storage_type storage): storage(std::move(storage)) { }
    base() = default;
  };
  

  template<class T, std::size_t level, std::size_t B, std::size_t L>
  struct node: base {
    static_assert(level > 0, "level error");
    static_assert(L > 0, "L error");
    static_assert(B > 0, "B error");

    static constexpr std::size_t max_size = 1ul << B;
    static_assert(max_size <= sizeof(std::size_t) * 8, "size error");
    
    static constexpr std::size_t shift = L + B * (level - 1ul);
    static constexpr std::size_t mask = (1ul << shift) - 1ul;

    struct split {
      const std::size_t sub;
      const std::size_t rest;
      
      split(std::size_t index):
        sub(index >> shift),
        rest(index & mask) {
        assert(sub < max_size);
      }
    };
  
    using child_type = node<T, level - 1, B, L>;
    using children_type = array<base>;
  
    children_type* children() const {
      return static_cast<children_type*>(base::storage.get());
    }

    child_type& child(std::size_t index) const {
      return static_cast<child_type&>(children()->at(index));
    }
  
    node(std::shared_ptr<children_type> children): base(std::move(children)) { }
    node() = default;
  
    T& get(split index) const {
      return child(index.sub).get(index.rest);
    }
  
    node set(split index, const T& value) const {
      if(children()->has(index.sub)) {
        return children()->set(index.sub, child(index.sub).set(index.rest, value));
      } else {
        return children()->set(index.sub, child_type(index.rest, value));
      }
    }

    node(split index, const T& value):
      base(children_type().set(index.sub, child_type(index.rest, value))) { }
  

    friend node emplace(node self, split index, const T& value) {
      if(!self.storage.unique() || !self.children()->has(index.sub)) {
        return self.set(index, value);
      }

      auto& child = self.child(index.sub);
      child = emplace(std::move(child), index.rest, value);
    
      return self;
    }
  

  };



  template<class T, std::size_t B, std::size_t L>
  struct node<T, 0, B, L>: base {
    static constexpr std::size_t level = 0;
    static constexpr std::size_t max_size = 1ul << L;
    static_assert(max_size <= sizeof(std::size_t) * 8, "size error");

    using children_type = array<T>;
    children_type* children() const {
      return static_cast<children_type*>(base::storage.get());
    }

    node(std::shared_ptr<children_type> children): base(std::move(children)) { }
    node() = default;
  
    T& get(std::size_t index) const {
      return children()->at(index);
    }

    node set(std::size_t index, const T& value) const {
      return children()->set(index, T(value));
    }

    node(std::size_t index, const T& value):
      base(children_type().set(index, T(value))) { }

    // node(std::size_t index, const T& value):
    //   base(make_array<T>(-1l)) {
    //   get(index) = value;
    // }

    
    friend node emplace(node self, std::size_t index, const T& value) {
      if(!self.storage.unique() || !self.children()->has(index)) {
        return self.set(index, value);
      }

      self.children()->at(index) = value;
    
      return self;
    }
  
  };


  template<class T, std::size_t B=6, std::size_t L=6>
  class amt {
    static constexpr std::size_t index_bits = sizeof(std::size_t) * 8;
    static constexpr std::size_t max_level = (index_bits - L) / B;

    using root_type = node<T, max_level, B, L>;
    root_type root;
  
    amt(root_type root): root(std::move(root)) { }
  
  public:
    amt() : root(nullptr) {}

    const T& get(std::size_t index) const {
      return root.get(index);
    }

    friend amt set(amt self, std::size_t index, const T& value) {
      if(!self.root.storage) return root_type(index, value);
      return emplace(std::move(self.root), index, value);
    }
  
  };

}


#endif
