#include "group.hpp"


template<class LHS, class RHS>
struct compose {
  using domain = typename RHS::domain;
  
  const LHS lhs;
  const RHS rhs;

  auto operator()(const domain& x) const {
    return lhs(rhs(x));
  }
  
  auto push(const domain& x) const {
    return lhs.push(rhs(x)) >> rhs.push(x);
  }
  
  auto pull(const domain& x) const {
    return rhs.pull(x) >> lhs.pull(rhs(x)) ;
  }
  
};

template<class LHS, class RHS>
static compose<LHS, RHS> operator>>(LHS lhs, RHS rhs) {
  return {lhs, rhs};
}

template<class G>
struct identity {
  using domain = G;
  G operator()(const domain& self) const { return self; }

  auto push(const domain& self) const {
    return identity<typename group<G>::algebra>();
  }

  auto pull(const domain& self) const {
    return identity<typename group<G>::algebra>();
  }
  
};


