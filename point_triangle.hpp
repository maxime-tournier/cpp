#ifndef POINT_TRIANGLE_HPP
#define POINT_TRIANGLE_HPP

#include <Eigen/Core>

template<class U>
class projector {
public:
  using real = U;
  using vec3 = Eigen::Matrix<real, 3, 1>;
private:
  vec3 p1, p2, p3, p1p2, p1p3;
  real distp1p2;
  real fa, fb, fc;
  real fdet;

  // singular case
  bool singular;
  real e0, e1, sign, ff;
  
  static constexpr real epsilon = 1e-10;
public:
  projector(vec3 p1, vec3 p2, vec3 p3):
      p1(p1),
      p2(p2),
      p3(p3),
      p1p2(p2 - p1),
      p1p3(p3 - p1),
      fa(p1p2.dot(p1p2)),
      fb(p1p2.dot(p1p3)),
      fc(p1p3.dot(p1p3)),
      fdet(fa * fc - fb * fb),
      singular(fdet < epsilon && fdet > -epsilon) {
    
  }

  static vec3 project_edge(vec3 origin, vec3 dir, vec3 p) {
    return origin + dir * dir.dot(p - origin) / dir.dot(dir);
  }

  vec3 operator()(vec3 p) const {
    if(singular) {
      if(fa + fc < epsilon) {
        // single point: easy peasy
        return p1;
      }

      if(fb > 0) {
        // both edges point in the same direction: project on the longest
        if(fa > fc) {
          return project_edge(p1, p1p2, p);
        } else {
          return project_edge(p1, p1p3, p);
        }
      } else {
        // edges pointing in opposite directions
        return project_edge(p2, p3 - p2, p);
      }
    }

    // non-singular case
    const vec3 pp1 = p1 - p;
    const real fd = p1p2.dot(pp1), fe = p1p3.dot(pp1);

    // minimize squared distance to source point
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


#endif
