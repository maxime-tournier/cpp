#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <Eigen/Core>
#include <Eigen/Geometry>

// types
template<class U, int M=Eigen::Dynamic, int N=Eigen::Dynamic>
using matrix = Eigen::Matrix<U, M, N>;

template<class U, int N=Eigen::Dynamic>
using vector = matrix<U, N, 1>;

template<class U, int N=Eigen::Dynamic>
using covector = matrix<U, 1, N>;

template<class U>
using quaternion = Eigen::Quaternion<U>;


template<class G>
struct group;

// rigid bodies
template<class U>
struct rigid {
  quaternion<U> rotation;
  vector<U, 3> translation;
  
  rigid(): rotation(quaternion<U>::Identity()), translation(0, 0, 0) { }
};


// quaternion group
template<class U>
struct group<quaternion<U>> {
  using type = quaternion<U>;
  using algebra = vector<U, 3>;
  
  static type id() { return type::Identity(); }
  static type inv(const type& self) { return self.conjugate(); }
  static type prod(const type& lhs, const type& rhs) { return lhs * rhs; }

  static type exp(const algebra& self);
  static algebra log(const type& self);

  static algebra dexp(const algebra& self, const algebra& dself);
  static algebra dlog(const type& self, const algebra& dself);
  
  static algebra Ad(const type& self, const algebra& omega) { return self * omega; }
  static algebra ad(const algebra& self, const algebra& omega) { return self.cross(omega); }
};


// product group, same types
template<class G, int N>
struct group<vector<G, N>> {
  static_assert(N != Eigen::Dynamic, "size must be known at compile-time");
  using type = vector<G, N>;
  using algebra = vector<typename group<G>::algebra, N>;

  template<class F, class ... Self>
  static auto map(F f, const Self& ... self) {
    using result_type = typename std::result_of<F(Self...)>::type;
    vector<result_type, N> result;
    for(int i = 0; i < N; ++i) {
      result(i) = f(self(i)...);
    }

    return result;
  }
  
  static type id() {
    return type::Constant(group<G>::id());
  }
  
  static type inv(const type& self) {
    return map(group<G>::inv, self);
  }
  
  static type prod(const type& lhs, const type& rhs) {
    return map(group<G>::prod, lhs, rhs);
  }
  
  static type exp(const algebra& self) {
    return map(group<G>::exp, self);
  }
  
  static algebra log(const type& self) {
    return map(group<G>::log, self);
  }
  
  static algebra dexp(const algebra& self, const algebra& dself) {
    return map(group<G>::dexp, self, dself);
  }
  
  static algebra dlog(const type& self, const algebra& dself) {
    return map(group<G>::dlog, self, dself);
  }
  
  static algebra Ad(const type& self, const algebra& omega) {
    return map(group<G>::Ad, self, omega);
  }
  
  static algebra ad(const algebra& self, const algebra& omega) {
    return map(group<G>::ad, self, omega);
  }
};


// product group, different types
template<class G, class A, class ... Gs>
struct product {
  using type = G;
  using algebra = A;
  
  static type id() {
    return group<G>::pack(group<Gs>::id()...);
  }

  static type inv(const type& self) {
    return group<G>::unpack(self, [](const Gs&...gs) {
      return group<G>::pack(group<Gs>::inv(gs)...);
    });
  }
  
  static type prod(const type& lhs, const type& rhs) {
    return group<G>::unpack(lhs, [&](const Gs& ... lhs) {
      return group<G>::unpack(rhs, [&](const Gs& ... rhs) {
        return group<G>::pack(group<Gs>::prod(lhs, rhs)...);
      });
    });
  }


  static type exp(const algebra& self) {
    return group<G>::unpack(self, [](const typename group<Gs>::algebra& ... ws) {
      return group<G>::pack(group<Gs>::exp(ws)...);
    });
  }

  static algebra log(const type& self) {
    return group<G>::unpack(self, [](const typename group<Gs>::algebra& ... ws) {
      return group<G>::pack(group<Gs>::log(ws)...);
    });
  }


  static algebra dexp(const algebra& self, const algebra& dself) {
    return group<G>::unpack(self, [&](const typename group<Gs>::algebra& ... ws) {
      return group<G>::unpack(dself, [&](const typename group<Gs>::algebra& ... dws) {
        return group<G>::pack(group<Gs>::dexp(ws, dws)...);
      });
    });
  }

  static algebra dlog(const type& self, const algebra& dself) {
    return group<G>::unpack(self, [&](const Gs& ... gs) {
      return group<G>::unpack(dself, [&](const typename group<Gs>::algebra& ... dgs) {
        group<G>::pack(group<Gs>::dlog(gs, dgs)...);
      });
    });
  }


  static algebra Ad(const type& self, const algebra& dself) {
    return group<G>::unpack(self, [&](const Gs& ... gs) {
      return group<G>::unpack(dself, [&](const typename group<Gs>::algebra& ... dgs) {
        return group<G>::pack(group<Gs>::Ad(gs, dgs)...);
      });
    });
  }

  static algebra ad(const type& self, const algebra& dself) {
    return group<G>::unpack(self, [&](const Gs& ... gs) {
      return group<G>::unpack(dself, [&](const typename group<Gs>::algebra& ... dgs) {
        group<G>::pack(group<Gs>::ad(gs, dgs)...);
      });
    });
  }
  
};


// eucliean spaces
template<class E>
struct euclidean {
  using type = E;
  using algebra = E;

  static type inv(const type& self) {
    return -self;
  }
  
  static type prod(const type& lhs, const type& rhs) {
    return lhs + rhs;
  }
  
  static type exp(const algebra& self) {
    return self;
  }
  
  static algebra log(const type& self) {
    return self;
  }
  
  static algebra dexp(const algebra& self, const algebra& dself) {
    return dself;
  }
  
  static algebra dlog(const type& self, const algebra& dself) {
    return dself;
  }
  
  static algebra Ad(const type& self, const algebra& omega) {
    return omega;
  }
  
  static algebra ad(const algebra& self, const algebra& omega) {
    return group<E>::id();
  }
  
};



// scalars
template<> struct group<double> : euclidean<double> {
  static double id() { return 0; }
};


// shorthands for real types
using real = double;

using vec = vector<real>;

using vec2 = vector<real, 2>;
using vec3 = vector<real, 3>;
using vec6 = vector<real, 6>;

using mat22 = matrix<real, 2, 2>;
using mat33 = matrix<real, 3, 3>;
using mat66 = matrix<real, 6, 6>;

using quat = quaternion<real>;






#endif
