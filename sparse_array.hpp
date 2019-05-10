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

  template<class ... Cases>
  struct overload: Cases... {
    using Cases::operator()...;
    overload(Cases...cases): Cases(cases)... { }
  };


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
  
  protected:
    template<class Ret, std::size_t ... M, class Func>
    Ret visit(std::index_sequence<M...>, const Func& func) const {
      using thunk_type = Ret (*) (const base&, const Func&);
    
      static const thunk_type table[] = {
        [](const base& self, const Func& func) -> Ret {
          return func(static_cast<const derived<T, M>&>(self));
        }...};
      
      return table[size()](*this, func);
    }

    static constexpr std::size_t default_size = sizeof(std::size_t) * 8;
  
  public:
  
    template<class Ret, std::size_t M=default_size, class ... Cases>
    Ret match(const Cases&... cases) const {
      return visit<Ret>(std::make_index_sequence<M>{}, overload<Cases...>(cases...));
    }

    template<std::size_t M=default_size>
    ref<base> set(std::size_t index, const T& value) const {
      return match<ref<base>>([&](const auto& self) -> ref<base> {
        using self_type = typename std::decay<decltype(self)>::type;
        return self.set(index, value,
                        std::make_index_sequence<self_type::static_size>{});
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
      assert(index < base<T>::default_size);
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
