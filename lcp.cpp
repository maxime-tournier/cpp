// -*- compile-command: "c++ -std=c++14 -O3 -DNDEBUG -ffast-math -march=native lcp.cpp -o lcp `pkg-config --cflags eigen3` -lstdc++" -*-

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

  static double clamp(double x) {
    return x;
  }

  // static real clamp(real x) {
  //   return std::max<real>(x, 0);
  // }


  static void fast_lcp(vec3& x, mat3x3 M, vec3 q) {
    const real x1[3] = {
      clamp(-q(0) / M(0, 0)),
      clamp(-q(1) / M(1, 1)),
      clamp(-q(2) / M(2, 2)),
    };

    using vec2i = Eigen::Matrix<Eigen::Index, 2, 1>;
    // TODO make this constexpr?
    const vec2i ind[3] =  {{1, 2}, {0, 2}, {0, 1}};
    
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
    
    // std::clog << lambda.transpose() << std::endl;
    // std::clog << (M * lambda + q).transpose() << std::endl;    
    // std::clog << lambda.dot(M * lambda + q) << std::endl;
    
    const vec3 origin = a;
    const vec3 delta = p - origin;
    const vec3 proj = p - n * n.dot(delta) / n.dot(n);

    return JT * lambda + proj;
  }


  struct projector_ref {
    vec3 p1, p2, p3, p1p2, p1p3;
    real distp1p2;
    real fa, fb, fc;
    real fdet;
    
    projector_ref(vec3 p1, vec3 p2, vec3 p3):
        p1(p1),
        p2(p2),
        p3(p3),
        p1p2(p2 - p1),
        p1p3(p3 - p1),
        distp1p2(p1p2.dot(p1p2)),
        fa(distp1p2),
        fb(p1p2.dot(p1p3)),
        fc(p1p3.dot(p1p3)),
        fdet(fa * fc - fb * fb) {

    }

    vec3 operator()(vec3 p) const {
      const vec3 pp1 = p1 - p;
      const real fd = p1p2.dot(pp1), fe = p1p3.dot(pp1);
      real fs = fb * fe - fc * fd, ft = fb * fd - fa * fe;

      if(fs + ft <= fdet) {
        if(fs < 0) {
          if(ft < 0) {
            // region 4
            if(fd < 0) {
              ft = 0;
              if(-fd >= fa) {
                fs = 1;
              } else {
                fs = -fd / fa;
              }
            } else {
              fs = 0;
              if(fe >= 0) {
                ft = 0;
              } else if(-fe >= fc) {
                ft = 1;
              } else {
                ft = -fe / fc;
              }
            }
          } else {
            // region 3
            fs = 0;
            if(fe >= 0) {
              ft = 0;
            } else if(-fe >= fc) {
              ft = 1;
            } else {
              ft = -fe / fc;
            }
          }
        } else if(ft < 0) {
          // region 5
          ft = 0;
          if(fd >= 0) {
            fs = 0;
          } else if(-fd >= fa) {
            fs = 1;
          } else {
            fs = -fd / fa;
          }

        } else {
          // region 0
          // minimum at interior point
          fs /= fdet;
          ft /= fdet;
        }
      } else {
        real ftmp0, ftmp1, fNumer, fDenom;

        if(fs < 0) {
          // region 2
          ftmp0 = fb + fd;
          ftmp1 = fc + fe;
          if(ftmp1 > ftmp0) {
            fNumer = ftmp1 - ftmp0;
            fDenom = fa - 2 * fb + fc;
            if(fNumer >= fDenom) {
              fs = 1;
              ft = 0;
            } else {
              fs = fNumer / fDenom;
              ft = 1 - fs;
            }
          } else {
            fs = 0;
            if(ftmp1 <= 0) {
              ft = 1;
            } else if(fe >= 0) {
              ft = 0;
            } else {
              ft = -fe / fc;
            }
          }
        } else if(ft < 0) {
          // region 6
          ftmp0 = fb + fe;
          ftmp1 = fa + fd;
          if(ftmp1 > ftmp0) {
            fNumer = ftmp1 - ftmp0;
            fDenom = fa - 2 * fb + fc;
            if(fNumer >= fDenom) {
              ft = 1;
              fs = 0;
            } else {
              ft = fNumer / fDenom;
              fs = 1 - ft;
            }
          } else {
            ft = 0;
            if(ftmp1 <= 0) {
              fs = 1;
            } else if(fd >= 0) {
              fs = 0;
            } else {
              fs = -fd / fa;
            }
          }
        } else {
          // region 1
          fNumer = fc + fe - fb - fd;
          if(fNumer <= 0) {
            fs = 0;
            ft = 1;
          } else {
            fDenom = fa - 2 * fb + fc;
            if(fNumer >= fDenom) {
              fs = 1;
              ft = 0;
            } else {
              fs = fNumer / fDenom;
              ft = 1 - fs;
            }
          }
        }
      }

      return (1 - fs - ft) * p1 + fs * p2 + ft * p3;
    }
    
  };
  
  static vec3 project_triangle_ref(vec3 p1, vec3 p2, vec3 p3, vec3 p) {
    const vec3 p1p2 = p2 - p1, p1p3 = p3 - p1, pp1 = p1 - p;
    const real distp1p2 = p1p2.dot(p1p2) // distp2p3 = (p3 - p2).dot(p3 - p2),
               // distp3p1 = p1p3.dot(p1p3)
      ;

    const real fa = distp1p2, fb = p1p2.dot(p1p3), fc = p1p3.dot(p1p3), fd = p1p2.dot(pp1),
      fe = p1p3.dot(pp1);

    const real fdet = fa * fc - fb * fb;
    real fs = fb * fe - fc * fd, ft = fb * fd - fa * fe;

    if(fs + ft <= fdet) {
      if(fs < 0) {
        if(ft < 0) {
          // region 4
          if(fd < 0) {
            ft = 0;
            if(-fd >= fa) {
              fs = 1;
            } else {
              fs = -fd / fa;
            }
          } else {
            fs = 0;
            if(fe >= 0) {
              ft = 0;
            } else if(-fe >= fc) {
              ft = 1;
            } else {
              ft = -fe / fc;
            }
          }
        } else {
          // region 3
          fs = 0;
          if(fe >= 0) {
            ft = 0;
          } else if(-fe >= fc) {
            ft = 1;
          } else {
            ft = -fe / fc;
          }
        }
      } else if(ft < 0) {
        // region 5
        ft = 0;
        if(fd >= 0) {
          fs = 0;
        } else if(-fd >= fa) {
          fs = 1;
        } else {
          fs = -fd / fa;
        }

      } else
      {
        // region 0        
        // minimum at interior point
        fs /= fdet;
        ft /= fdet;
      }
    } else {
      real ftmp0, ftmp1, fNumer, fDenom;

      if(fs < 0) 
        {
          // region 2
          ftmp0 = fb + fd;
          ftmp1 = fc + fe;
          if(ftmp1 > ftmp0) {
            fNumer = ftmp1 - ftmp0;
            fDenom = fa - 2 * fb + fc;
            if(fNumer >= fDenom) {
              fs = 1;
              ft = 0;
            } else {
              fs = fNumer / fDenom;
              ft = 1 - fs;
            }
          } else {
            fs = 0;
            if(ftmp1 <= 0) {
              ft = 1;
            } else if(fe >= 0) {
              ft = 0;
            } else {
              ft = -fe / fc;
            }
          }
        } else if(ft < 0) 
        {
          // region 6          
          ftmp0 = fb + fe;
          ftmp1 = fa + fd;
          if(ftmp1 > ftmp0) {
            fNumer = ftmp1 - ftmp0;
            fDenom = fa - 2 * fb + fc;
            if(fNumer >= fDenom) {
              ft = 1;
              fs = 0;
            } else {
              ft = fNumer / fDenom;
              fs = 1 - ft;
            }
          } else {
            ft = 0;
            if(ftmp1 <= 0) {
              fs = 1;
            } else if(fd >= 0) {
              fs = 0;
            } else {
              fs = -fd / fa;
            }
          }
        } else
        {
          // region 1          
          fNumer = fc + fe - fb - fd;
          if(fNumer <= 0) {
            fs = 0;
            ft = 1;
          } else {
            fDenom = fa - 2 * fb + fc;
            if(fNumer >= fDenom) {
              fs = 1;
              ft = 0;
            } else {
              fs = fNumer / fDenom;
              ft = 1 - fs;
            }
          }
        }
    }
    
    return (1 - fs - ft) * p1 + fs * p2 + ft * p3;    
  }

  struct projector_alt {
    mat3x3 points;
    mat3x3 edges;    
    mat3x3 points_inv;
    vec3 n;
    real n2;
    
    projector_alt(vec3 a, vec3 b, vec3 c) {
      points << a, b, c;
      edges << c - b, a - c, b - a;
    
      points_inv = points.inverse();
      n = edges.col(0).cross(edges.col(1));
      n2 = n.dot(n);
      
    }

    vec3 operator()(vec3 q) const {
      const vec3 delta = q - points.col(0);
      const vec3 proj = q - n * n.dot(delta) / n2;
      const vec3 coords = points_inv * proj;

      int negative = 0;

      // negative, positive
      int last_index[2];
      for(int i = 0; i < 3; ++i) {
        const auto neg = coords[i] < 0;
        negative += neg;
        last_index[neg] = i;
      }

      switch(negative) {
      case 0: return proj;
      case 1: {
        const vec3 e = edges.col(last_index[0]);
        const vec3 origin = points.col(last_index[1]);
        const vec3 delta = q - origin;
      
        real alpha = e.dot(delta) / e.dot(e);

        // TODO branchless
        // alpha = (alpha > 0) * alpha - (alpha > 1) * (alpha - 1);
        alpha = std::max<real>(0, alpha);
        alpha = std::min<real>(1, alpha);

        return origin + e * alpha;
      }      
      case 2: return points.col(last_index[1]);
      default:
        throw std::logic_error("unreachable");
      }
    }
  };

  
  static vec3 project_triangle_alt(vec3 a, vec3 b, vec3 c, vec3 q) {
    mat3x3 points;
    points << a, b, c;

    mat3x3 edges;
    edges << c - b, a - c, b - a;
    
    const vec3 n = edges.col(0).cross(edges.col(1));
    
    const vec3 origin = a;
    const vec3 delta = q - origin;
    const vec3 proj = q - n * n.dot(delta) / n.dot(n);

    const vec3 coords = points.inverse() * proj;

    int negative = 0;

    // negative, positive
    int last_index[2];
    for(int i = 0; i < 3; ++i) {
      const auto neg = coords[i] < 0;
      negative += neg;
      last_index[neg] = i;
    }

    switch(negative) {
    case 0: return proj;
    case 1: {
      const vec3 e = edges.col(last_index[0]);
      const vec3 origin = points.col(last_index[1]);
      const vec3 delta = q - origin;
      
      real alpha = e.dot(delta) / e.dot(e);
      // alpha = (alpha > 0) * alpha - (alpha > 1) * (alpha - 1);
      alpha = std::max<real>(0, alpha);
      alpha = std::min<real>(1, alpha);

      return origin + e * alpha;
    }      
    case 2: return points.col(last_index[1]);
    default:
      throw std::logic_error("unreachable");
    }
    
  }
};

#include "timer.hpp"
#include <bitset>

Eigen::Matrix<double, 3, 1> solution;

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
    
    std::clog << "=================" << std::endl;

    
    static const auto make_problem = [](auto k) {
      closest::vec3 a, b, c, p;
      a.setRandom();
      b.setRandom();
      c.setRandom();

      p.setRandom();

      return k(a, b, c, p);
    };

    static const int n = 1000000;

    const auto lcp_duration = with_time([] {
      for(int i = 0; i < n; ++i) {
        solution = make_problem([](auto a, auto b, auto c, auto p) {
          return closest::project_triangle(a, b, c, p);
        });
      }
    });

    const auto alt_duration = with_time([] {
      for(int i = 0; i < n; ++i) {
        solution = make_problem([](auto a, auto b, auto c, auto p) {
          return closest::project_triangle_alt(a, b, c, p);
        });
      }
    });

    const auto ref_duration = with_time([] {
      for(int i = 0; i < n; ++i) {
        solution = make_problem([](auto a, auto b, auto c, auto p) {
          return closest::project_triangle_ref(a, b, c, p);
        });
      }
    });
    
    std::clog << "lcp: " << lcp_duration << std::endl;
    std::clog << "alt: " << alt_duration << std::endl;
    std::clog << "ref: " << ref_duration << std::endl;        

    ////////////////////////////////////////////////////////////////////////////////
    const int m = 100;
    const auto proj_alt_duration = with_time([] {
      std::vector<closest::projector_alt> projs;
      
      for(int i = 0; i < n; ++i) {
        projs.emplace_back(closest::vec3::Random(),
                           closest::vec3::Random(),
                           closest::vec3::Random());
      }

      for(auto& proj: projs) {
        for(int i = 0; i < m; ++i) {
          solution = proj(closest::vec3::Random());
        }
      }
    });


    const auto proj_ref_duration = with_time([] {
      std::vector<closest::projector_ref> projs;
      
      for(int i = 0; i < n; ++i) {
        projs.emplace_back(closest::vec3::Random(),
                           closest::vec3::Random(),
                           closest::vec3::Random());
      }

      for(auto& proj: projs) {
        for(int i = 0; i < m; ++i) {
          solution = proj(closest::vec3::Random());
        }
      }
    });

    std::clog << "proj alt: " << proj_alt_duration << std::endl;
    std::clog << "proj ref: " << proj_ref_duration << std::endl;        
    
    
    // closest::mat3x3 B;
    // B.col(0) = a;
    // B.col(1) = b;
    // B.col(2) = c;

    // const closest::vec3 coords = B.inverse() * x;
    // std::clog << "coords sum: " << coords.sum() << std::endl;
    // std::clog << "coords: " << coords.transpose() << std::endl;
  }  
  
  return 0;
}
