// -*- compile-command: "c++ -std=c++14 -O3 -march=native lcp.cpp -o lcp `pkg-config --cflags eigen3` -lstdc++" -*-

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


#include <iostream>


int main(int argc, char** argv) {
  using lcp = struct lcp<double>;

  std::srand(time(0));

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
  
  return 0;
}
