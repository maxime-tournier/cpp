#include <memory>


template<class T>
struct base {
  const std::size_t mask;
  T data[0];
  
  base(std::size_t mask): mask(mask) { }

  T& get(std::size_t index) const {
    
  }
  
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

template<class T, std::size_t M, class...Args>
static std::shared_ptr<base<T>> make_array(std::size_t size, Args&& ... args) {
  using thunk_type = std::shared_ptr<base<T>> (*)(Args&&...);
  
  static const auto table =
    unpack_sequence<thunk_type>(std::make_index_sequence<M>{}, [](auto index) -> thunk_type {
      using index_type = decltype(index);
      return +[](Args&&...args) -> std::shared_ptr<base<T>> {
        return std::make_shared<derived<T, index_type::value>>(std::forward<Args>(args)...);
      };
    });
  
  return table[size](std::forward<Args>(args)...);
};




static const struct test {
  test() {
    auto x = make_array<double, 32>(0, 0ul);
  }
} instance;


