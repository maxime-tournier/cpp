// -*- compile-command: "c++ -std=c++14 -O3 -march=native lcp.cpp -o lcp `pkg-config --cflags eigen3` -lstdc++" -*-

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <iostream>
#include <array>

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
    assert(M(0, 0) > 0);
    x(0) = std::max<real>(0, -q(0) / M(0, 0));
  }

  template<int N>
  static void solve(vector<N>& x, matrix<N> M, vector<N> q) {
    static_assert(N > 1, "size error");
    indices<N - 1> sub;
    vector<N> w;

    std::array<bool, N> active;
    
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
      active[i] = w(i) == 0;
    }

    for(int i = 0; i < N; ++i) {
      if(!active[i]) {
        M.row(i) = vector<N>::Zero().transpose();
        M.col(i) = vector<N>::Zero();
        M(i, i) = 1;
        q(i) = w(i);
      }
    }
    
    // TODO use LLT when N > 4?
    x.noalias() = M.inverse() * (w - q);
  }
};


template<class U>
struct closest {
  using real = U;
  using vec3 = Eigen::Matrix<real, 3, 1>;
  using vec2 = Eigen::Matrix<real, 2, 1>;
  
  using mat3x3 = Eigen::Matrix<real, 3, 3>;
  using mat2x2 = Eigen::Matrix<real, 2, 2>;  

  static vec3 project_triangle(vec3 a, vec3 b, vec3 c, vec3 p) {
    const vec3* points[3] = {&a, &b, &c};
    
    const vec3 edges[3] = {
      b - a,
      c - b,
      a - c
    };

    const vec3 n = edges[0].cross(edges[1]);
    
    mat3x3 JT;
    for(int i = 0; i < 3; ++i) {
      JT.col(i) = n.cross(edges[i]);
    }
    
    vec3 bounds;

    for(int i = 0; i < 3; ++i) {
      bounds(i) = JT.col(i).dot(*points[i]);
    }
    
    const mat3x3 M = JT.transpose() * JT;
    const vec3 q = (JT.transpose() * p - bounds);

    vec3 lambda;
    lcp<real>::solve(lambda, M, q);
    std::clog << lambda.transpose() << std::endl;
    std::clog << (M * lambda + q).transpose() << std::endl;    
    std::clog << lambda.dot(M * lambda + q) << std::endl;
    
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
    std::clog << "=================" << std::endl;
    using closest = struct closest<double>;
    closest::vec3 a, b, c, p;
    a.setRandom();
    b.setRandom();
    c.setRandom();

    p.setRandom();

    const closest::vec3 x = closest::project_triangle(a, b, c, p);

    closest::mat3x3 B;
    B.col(0) = a;
    B.col(1) = b;
    B.col(2) = c;

    const closest::vec3 coords = B.inverse() * x;
    std::clog << "coords sum: " << coords.sum() << std::endl;
    std::clog << "coords: " << coords.transpose() << std::endl;
  }  
  
  return 0;
}
