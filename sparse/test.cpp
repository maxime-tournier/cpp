#include <memory>



namespace sparse {

  static std::size_t size(std::size_t mask) {
    return __builtin_popcountl(mask);
  }
  
  static std::size_t index(std::size_t mask, std::size_t index) {
    return size(mask & ((1ul << index) - 1));      
  }
  
}


template<class T>
struct base {
  const std::size_t mask;
  T data[0];
  
  base(std::size_t mask): mask(mask) { }

  std::size_t size() const { return sparse::size(mask); }
  T& get(std::size_t index) { return data[sparse::index(mask, index)]; }

  template<std::size_t M>
  std::shared_ptr<base> set(std::size_t index, T&& value) const;
};


template<class T, std::size_t N>
struct derived: base<T> {
  T values[N];
  
  derived(std::size_t mask): base<T>(mask) { }
};


template<class Ret, std::size_t...Ms, class Cont>
static std::array<Ret, sizeof...(Ms)> unpack_sequence(std::index_sequence<Ms...>, Cont cont) {
  return {cont(std::integral_constant<std::size_t, Ms>{})...};
};

template<class T, std::size_t M>
static std::shared_ptr<base<T>> make_array(std::size_t mask) {
  using thunk_type = std::shared_ptr<base<T>> (*)(std::size_t mask);
  
  static const auto table =
    unpack_sequence<thunk_type>(std::make_index_sequence<M>{}, [](auto index) -> thunk_type {
      using index_type = decltype(index);
      return +[](std::size_t mask) -> std::shared_ptr<base<T>> {
        return std::make_shared<derived<T, index_type::value>>(mask);
      };
    });
  
  return table[sparse::size(mask)](mask);
};



template<class T>
template<std::size_t M>
std::shared_ptr<base<T>> base<T>::set(std::size_t index, T&& value) const {
  const std::size_t bit = 1ul << index;
  
  auto res = make_array<T, M>(mask | (1ul << index));

  const std::size_t s = sparse::index(res->mask, index);
  for(std::size_t i = 0; i < s; ++i) {
    res->data[i] = data[i];
  }

  res->data[s] = std::move(value);

  const bool insert = mask & bit;
  for(std::size_t i = s + 1, n = res->size(); i < n; ++i) {
    res->data[insert ? i + 1 : i] = data[i];
  }

  return res;
}


#include <iostream>

static const struct test {
  test() {
    auto x = make_array<double, 32>(0ul);
    x = x->set<32>(0, 1);
    x = x->set<32>(4, 2);
    std::clog << x->get(0) << std::endl;
    std::clog << x->get(4) << std::endl;    
    std::exit(0);
  }
} instance;


