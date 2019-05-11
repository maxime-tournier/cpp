#ifndef SPARSE_ARRAY_HPP
#define SPARSE_ARRAY_HPP

#include <memory>
#include <cassert>
#include <iostream>

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
  
  
  template<class T>
  struct base {
    const std::size_t mask;
    const T data[0];

    const T* begin() const { return data; }
    const T* end() const { return data + size(); }  

    std::size_t size() const { return __builtin_popcount(mask); }
  
    std::size_t sparse_index(std::size_t index) const {
      return __builtin_popcount(mask & ((1 << (index + 1)) - 1));
    }

    bool contains(std::size_t index) const { return mask & (1 << index); }
  
    const T& get(std::size_t index) const {
      assert(contains(index));
      return data[sparse_index(index)];
    }
  
  public:
    template<std::size_t M>
    ref<base> set(std::size_t index, const T& value) const {
      return visit<ref<base>>(std::make_index_sequence<M>{}, *this, [&](const auto& self) {
        using self_type = typename std::decay<decltype(self)>::type;
        return self.set(index, value, std::make_index_sequence<self_type::static_size>{});
      });
    }
  
  };


  template<class T, std::size_t N>
  struct derived: base<T> {
    static constexpr std::size_t static_size = N;
    T storage[N];
  
    template<class...Args>
    derived(std::size_t mask, Args&&...args):
      base<T>{mask},
      storage{std::forward<Args>(args)...} {
        assert(__builtin_popcount(mask) == N);
      }
  
    template<std::size_t ... Ms>
    ref<base<T>> set(std::size_t index, const T& value,
                     std::index_sequence<Ms...>) const {
      if(this->mask & (1 << index)) {
        auto res = std::make_shared<derived<T, N>>(this->mask, storage[Ms]...);
        res->storage[this->sparse_index(index)] = value;
        return res;
      } else {
        auto res = std::make_shared<derived<T, N + 1>>(this->mask | (1 << index),
                                                       storage[Ms]..., value);
        std::swap(res->storage[this->sparse_index(index)], res->storage[N]);
        return res;
      }
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
