// -*- compile-command: "CXXFLAGS=-std=c++14 make const_array" -*-

#include <memory>
#include <iostream>

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
  const std::size_t size;
  const T data[0];

  const T* begin() const { return data; }
  const T* end() const { return data + size; }  
  
private:
  template<std::size_t ... M, class Func, class ... Args>
  void visit(std::index_sequence<M...>, const Func& func, Args&&...args) const {
    using thunk_type = void(*) (const base*, const Func&, Args&&...);
    
    static const thunk_type table[] = {[](const base* self, const Func& func, Args&&...args) {
      return func(static_cast<const derived<T, M>*>(self), std::forward<Args>(args)...);
    }...};

    return table[size](this, func, std::forward<Args>(args)...);
  }

  static constexpr std::size_t default_size = 32;

public:
  
  template<std::size_t M=default_size, class ... Cases>
  friend void match(const ref<base>& self, const Cases&... cases) {
    return self->visit(std::make_index_sequence<M>{}, overload<Cases...>(cases...));
  }
      

  template<std::size_t M=default_size>
  friend ref<base> push(const ref<base>& self, const T& value) {
    ref<base> res;
    match(self, [&](auto self) {
      res = self->push(value, std::make_index_sequence<self->static_size>{});
    });

    return res;
  }

  template<std::size_t M=default_size>
  friend ref<base> pop(const ref<base>& self) {
    ref<base> res;
    match(self, [&](auto self) {
      static_assert(self->static_size > 0, "empty array");
      res = self->pop(std::make_index_sequence<self->static_size - 1>{});
    }, [](const derived<T>* self) {
      throw std::runtime_error("empty array");
    });
  
    return res;
  }
  
};


template<class T, std::size_t N>
struct derived: base<T> {
  static constexpr std::size_t static_size = N;
  const T storage[N];

  template<class...Args>
  derived(Args&&...args): base<T>{N}, storage{std::forward<Args>(args)...} { }
  
  template<std::size_t ... Ms>
  ref<base<T>> push(const T& value, std::index_sequence<Ms...>) const {
    return std::make_shared<derived<T, N + 1>>(storage[Ms]..., value);
  }

  template<std::size_t ... Ms>
  ref<base<T>> pop(std::index_sequence<Ms...>) const {
    static_assert(N > 0, "empty array");
    return std::make_shared<derived<T, N - 1>>(storage[Ms]...);
  }

};



int main(int, char**) {
  using type = double;

  auto d = std::make_shared<derived<type>>();
  ref<base<type>> b = d;

  std::cout << d->data << ", " << d->storage << std::endl;

  match(b, [](auto self) {
    std::clog << sizeof(self->storage) / sizeof(type) << std::endl;
  });

  auto p = push(b, 2.0);

  for(auto i: *p) {
    std::cout << i << ", ";
  }

  std::cout << std::endl;
  
  auto q = pop(p);
  for(auto i: *q) {
    std::cout << i << ", ";
  }

  
  return 0;
}
