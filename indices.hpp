#ifndef INDICES_HPP
#define INDICES_HPP

#include <cstddef>

// a compile-time index sequence similar to c++14 std::index_sequence

template<std::size_t ... N>
struct indices { };

namespace detail {
  template<class s1, class s2>
  struct merge_indices_type;

  template<std::size_t ... I1, std::size_t ... I2>
  struct merge_indices_type< indices<I1...>, indices<I2...> >{
    using type = indices<I1..., I2...>;
  };

  template<std::size_t S, std::size_t E>
  struct make_indices_type {

    static constexpr std::size_t pivot = (E + S) / 2;
    using lhs_type = typename make_indices_type<S, pivot>::type;
    using rhs_type = typename make_indices_type<pivot, E>::type;    
    
    using type = typename merge_indices_type< lhs_type, rhs_type>::type;
  };

  template<std::size_t I>
  struct make_indices_type<I, I> {
    using type = indices<>;
  };

  template<std::size_t I>
  struct make_indices_type<I, I + 1> {
    using type = indices<I>;
  };
  

}




template<std::size_t N>
using range_indices = typename detail::make_indices_type<0, N>::type;

template<class ... T>
using type_indices = range_indices< sizeof...(T) >;




#endif
