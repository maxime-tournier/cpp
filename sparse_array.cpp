// -*- compile-command: "CXXFLAGS='-std=c++14 -g' make sparse_array" -*-

#include <memory>
#include <iostream>
#include <cassert>

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
    return __builtin_popcount(mask & (1 << (index + 1) - 1));
  }

  bool has(std::size_t index) const { return mask & (1 << index); }
  
  const T& get(std::size_t index) const {
    assert(has(index));
    return data[sparse_index(index)];
  }
  
protected:
  template<std::size_t ... M, class Func, class ... Args>
  void visit(std::index_sequence<M...>, const Func& func, Args&&...args) const {
    using thunk_type = void(*) (const base*, const Func&, Args&&...);
    
    static const thunk_type table[] = {[](const base* self, const Func& func, Args&&...args) {
      return func(static_cast<const derived<T, M>*>(self), std::forward<Args>(args)...);
    }...};

    return table[size()](this, func, std::forward<Args>(args)...);
  }

  static constexpr std::size_t default_size = sizeof(std::size_t) * 8;
  
public:
  
  template<std::size_t M=default_size, class ... Cases>
  void match(const Cases&... cases) const {
    return visit(std::make_index_sequence<M>{}, overload<Cases...>(cases...));
  }

  template<std::size_t M=default_size>
  ref<base> set(std::size_t index, const T& value) const {
    ref<base> res;
    match([&](auto self) {
      res = self->set(index, value, std::make_index_sequence<self->static_size>{});
    });
    return res;
  }
  
};


template<class T, std::size_t N>
struct derived: base<T> {
  static constexpr std::size_t static_size = N;
  T storage[N];
  
  template<class...Args>
  derived(std::size_t mask=0, Args&&...args):
    base<T>{mask},
    storage{std::forward<Args>(args)...} {
    assert(__builtin_popcount(mask) == N);
  }
  
  template<std::size_t ... Ms>
  ref<base<T>> set(std::size_t index, const T& value, std::index_sequence<Ms...>) const {
    assert(index < base<T>::default_size);
    if(this->mask & (1 << index)) {
      auto res = std::make_shared<derived<T, N>>(this->mask, storage[Ms]...);
      res->storage[this->sparse_index(index)] = value;
      return res;
    } else {
      auto res = std::make_shared<derived<T, N + 1>>(this->mask | (1 << index), storage[Ms]..., value);
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


int main(int, char**) {
  using type = double;

  auto d = std::make_shared<derived<type>>();
  ref<base<type>> b = d;

  auto p = b->set(7, 2.0);
  std::cout << *p << std::endl;
  
  auto q = p->set(2, 1.0);
  std::cout << *q << std::endl;    

  
  return 0;
}
