#ifndef INDICES_HPP
#define INDICES_HPP

#include <tuple>

template<std::size_t ... I>
struct indices {
  template<std::size_t J>
  using push = indices<I..., J>;
};

template<class ... T>
struct tuple_indices;

template<>
struct tuple_indices<> {
  using type = indices<>;
};

template<class H, class ... T>
struct tuple_indices<H, T...> {
  using type = typename tuple_indices<T...>::type::template push<1 + sizeof...(T)>;
};


template<class ... T>
using indices_for = typename tuple_indices<T...>::type;




#endif
