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

  static real clamp(real x) {
    return std::max<real>(x, 0);
  }

  static void fast_lcp(vec3& x, mat3x3 M, vec3 q) {
    const real x1[3] = {
      clamp(-q(0) / M(0, 0)),
      clamp(-q(1) / M(1, 1)),
      clamp(-q(2) / M(2, 2)),
    };

    using vec2i = Eigen::Matrix<Eigen::Index, 2, 1>;
    // TODO make this constexpr?
    const vec2i ind[3] = {{1, 2}, {0, 2}, {0, 1}};
    
    mat2x2 M2inv[3];
    
    vec2 x2[3];
    vec3 w;
    int skip = 0;
    for(int i = 0; i < 3; ++i) {
      mat2x2 sub;               // TODO optimize (symmetric)
      for(int j = 0; j < 2; ++j) {
        sub.col(j) = ind[i].unaryExpr(M.col(ind[i][j]));
      }

      vec2 w2;
      for(int j = 0; j < 2; ++j) {
        const int other = (j + 1) % 2;
        // w2(j) = clamp(sub(j, other) * x1[ind[i][other]] + q(ind[i][j]));
        w2(j) = clamp(M(ind[i][j], ind[i][other]) * x1[ind[i][other]] + q(ind[i][j]));        
      }

      // TODO optimize?
      M2inv[i] = sub.inverse();
      x2[i].noalias() = M2inv[i] * (w2 - ind[i].unaryExpr(q));
      
      w[i] = clamp(ind[i].unaryExpr(M.col(i)).dot(x2[i]) + q(i));

      // TODO branchless?
      if(w[i] > 0) {
        skip = i;
      }
    }

    const vec2 res = M2inv[skip] * ind[skip].unaryExpr(w - q);
    x[ind[skip][0]] = res[0];
    x[ind[skip][1]] = res[1];    
    x[skip] = 0;
  }
  
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
    // lcp<real>::solve(lambda, M, q);
    fast_lcp(lambda, M, q);
    
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
