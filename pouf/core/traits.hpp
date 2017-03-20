#ifndef POUF_TRAITS_HPP
#define POUF_TRAITS_HPP

template<class G, class = void> struct traits;

template<class G>
using deriv = typename traits<G>::deriv_type;

template<class G>
using scalar = typename traits<G>::scalar_type;


template<class G>
static G id() { return traits<G>::id(); }

template<class G>
static G inv(const G& g) { return traits<G>::inv(g); }

template<class G>
static G prod(const G& g, const G& h) { return traits<G>::prod(g, h); }


template<class G>
static G exp(const deriv<G>& v) { return traits<G>::exp(v); }


template<class G>
static deriv<G> AdT(const G& g, const deriv<G>& x) {
  return traits<G>::AdT(g, x);
}


#endif
