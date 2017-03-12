#ifndef POUF_TRAITS_HPP
#define POUF_TRAITS_HPP

template<class G, class = void> struct traits;

template<class G>
using deriv = typename traits<G>::deriv;

template<class G>
using scalar = typename traits<G>::scalar;




#endif
