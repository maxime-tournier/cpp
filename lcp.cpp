// -*- compile-command: "c++ -std=c++14 -O3 -march=native lcp.cpp -o lcp
// `pkg-config --cflags eigen3` -lstdc++" -*-

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <iostream>

template<class U>
struct lcp {
  using real = U;

  template<int N>
  using vector = Eigen::Matrix<real, N, 1>;

  template<int N>
  using indices = Eigen::Matrix<Eigen::Index, N, 1>;

  template<int N>
  using matrix = Eigen::Matrix<real, N, N>;


  static void solve(vector<1>& x, const matrix<1>& M, const vector<1>& q) {
    x(0) = std::max<real>(0, -q(0) / M(0, 0));
  }

  template<int N>
  static void solve(vector<N>& x, const matrix<N>& M, const vector<N>& q) {
    static_assert(N > 1, "size error");
    indices<N - 1> sub;
    vector<N> w;

    for(std::size_t i = 0; i < N; ++i) {
      // filter all but ith component
      auto ptr = sub.data();
      for(std::size_t j = 0; (j += (j == i)) < N; ++j, ++ptr) {
        *ptr = j;
      }

      vector<N - 1> qq = sub.unaryExpr(q);
      matrix<N - 1> MM;
      for(int j = 0; j < N - 1; ++j) {
        MM.col(j) = sub.unaryExpr(M.col(sub(j)));
      }

      vector<N - 1> xx;
      solve(xx, MM, qq);

      w(i) = std::max<real>(sub.unaryExpr(M.col(i)).dot(xx) + q(i), 0);
    }

    // TODO use LLT when N > 4?
    x.noalias() = M.inverse() * (w - q);
  }
};


template<class U>
struct closest {
  using real = U;
  using vec3 = Eigen::Matrix<real, 3, 1>;
  using mat3x3 = Eigen::Matrix<real, 3, 3>;


  static vec3 project_triangle(vec3 a, vec3 b, vec3 c, vec3 p) {
    const vec3 u = b - a;
    const vec3 v = c - b;
    const vec3 w = a - c;

    const vec3 n = u.cross(v);
    const vec3 u_bar = u.cross(n);
    const vec3 v_bar = v.cross(n);
    const vec3 w_bar = w.cross(n);

    mat3x3 JT;
    JT << -u_bar, -v_bar, -w_bar;

    vec3 bound;
    bound << -u_bar.dot(a), -v_bar.dot(b), -w_bar.dot(c);

    const mat3x3 M = JT.transpose() * JT;
    const vec3 q = JT.transpose() * p - bound;

    vec3 lambda;
    lcp<real>::solve(lambda, M, q);

    const vec3 origin = a;
    const vec3 delta = p - origin;
    const vec3 proj = origin + (delta - n * n.dot(delta) / n.dot(n));

    return JT * lambda + proj;
  }
};



int main(int argc, char** argv) {
  std::srand(time(0));

  {
    using lcp = struct lcp<double>;

    static constexpr int N = 3;

    lcp::matrix<N> M;
    lcp::vector<N> q, x, w;

    M.setRandom();
    M = M.transpose() * M;

    q.setRandom();
    lcp::solve(x, M, q);

    w.noalias() = M * x + q;

    std::cout << "x: " << x.transpose() << std::endl;
    std::cout << "w: " << w.transpose() << std::endl;
    std::cout << "xw: " << x.dot(w) << std::endl;
  }
  
  {
    using closest = struct closest<double>;
    closest::vec3 a, b, c, p;
    a.setRandom();
    b.setRandom();
    c.setRandom();

    p.setRandom();

    const closest::vec3 x = closest::project_triangle(a, b, c, p);

    closest::mat3x3 B;
    B << a, b, c;

    const closest::vec3 coords = B.inverse() * x;
    std::clog << coords.sum() << std::endl;
    std::clog << coords.transpose() << std::endl;
  }  
  
  return 0;
}
