#ifndef SPARSE_ARRAY_HPP
#define SPARSE_ARRAY_HPP

#include <memory>
#include <cassert>
#include <iostream>

#include <bitset>               // debug

namespace sparse {
  
  template<class T> struct base;
  template<class T, std::size_t N=0> struct derived;

  template<class T>
  using ref = std::shared_ptr<T>;
  

  template<class Ret, class T, std::size_t ... Ms, class Func>
  static Ret visit(std::index_sequence<Ms...>, const base<T>& self, const Func& func) {
    using thunk_type = Ret (*) (const base<T>&, const Func&);
    
    static const thunk_type table[] = {[](const base<T>& self, const Func& func) -> Ret {
      return func(static_cast<const derived<T, Ms>&>(self));
    }...};
      
    return table[self.size()](self, func);
  }

  
  static std::size_t sparse_index(std::size_t mask, std::size_t index) {
    const std::size_t res = __builtin_popcount(mask & ((1ul << index) - 1));      
    return res;
  }
  
  
  template<class T>
  struct base {
    const std::size_t mask;
    const T data[0];

    const T* begin() const { return data; }
    const T* end() const { return data + size(); }  

    std::size_t size() const { return __builtin_popcount(mask); }
  
    bool contains(std::size_t index) const { return mask & (1ul << index); }
  
    const T& get(std::size_t index) const {
      assert(contains(index));
      return data[sparse_index(mask, index)];
    }
  
  public:
    template<std::size_t M>
    ref<base> set(std::size_t index, const T& value) const {
      return visit<ref<base>>(std::make_index_sequence<M>{}, *this, [&](const auto& self) {
        using self_type = typename std::decay<decltype(self)>::type;
        return self.set(index, value, std::make_index_sequence<self_type::static_size>{});
      });
    }

    template<std::size_t M>
    ref<base> add(std::size_t index, const T& value) const {
      return visit<ref<base>>(std::make_index_sequence<M>{}, *this, [&](const auto& self) {
        using self_type = typename std::decay<decltype(self)>::type;
        return self.add(index, value, std::make_index_sequence<self_type::static_size + 1>{});
      });
    }

    
  };


  template<class T, std::size_t N>
  struct derived: base<T> {
    static constexpr std::size_t static_size = N;
    const std::array<T, N> storage;
  
    template<class...Args>
    derived(std::size_t mask, Args&&...args):
      base<T>{mask},
      storage{std::forward<Args>(args)...} {
        assert(__builtin_popcount(mask) == N);
      }

    template<std::size_t ... Ms>
    ref<base<T>> set(std::size_t index, const T& value,
                     std::index_sequence<Ms...>) const {
      assert(this->mask & (1ul << index));
      const std::size_t sparse = sparse_index(this->mask, index);
      return std::make_shared<derived>(this->mask, (Ms == sparse ? value : storage[Ms])...);
    }
    

    template<std::size_t ... Ms>
    ref<base<T>> add(std::size_t index, const T& value,
                     std::index_sequence<Ms...>) const {
      assert(!(this->mask & (1ul << index)));
      const std::size_t new_mask = this->mask | (1 << index);
      const std::size_t sparse = sparse_index(new_mask, index);
      return std::make_shared<derived<T, N + 1>>(new_mask,
                                                 (Ms < sparse ? storage[Ms] :
                                                  Ms == sparse ? value : storage[Ms - 1])...);
    }

  };


  template<class T>
  static std::ostream& operator<<(std::ostream& out, const base<T>& self) {
    bool first = true;
    for(auto i: self) {
      if(first) first = false;
      else out << ", ";
      out << i;
    }

    return out;
  }
  
}

#endif
