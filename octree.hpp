#ifndef CPP_OCTREE_HPP
#define CPP_OCTREE_HPP

#include <vector>
#include <bitset>

#include <algorithm>

#include <iostream>

#include <Eigen/Core>



template<class T>
struct cell {

  static constexpr std::size_t brute_force_threshold = 128;
  
  static constexpr std::size_t max_level = (8 * sizeof(T)) / 3;
  using coord = std::bitset<max_level>;
  
  static constexpr T resolution() { return 1ul << max_level; }

  static constexpr std::size_t size = 3 * max_level;
  using bits_type = std::bitset<size>;
  bits_type bits;
  
  // static constexpr std::size_t padding = (8 * sizeof(T)) % 3;  

  template<class Func>
  void decode(Func&& func) const {
    coord x, y, z;

    auto value = bits.to_ulong();
    for(std::size_t i = 0; i < max_level; ++i) {
      x[i] = value & 4;
      y[i] = value & 2;
      z[i] = value & 1;

      value >>= 3;
    }

    func(x, y, z);
  }

  static bits_type encode(const coord& x, const coord& y, const coord& z) {
    T value = 0;
    for(std::size_t i = 0; i < max_level; ++i) {
      const std::size_t j = max_level - 1 - i;
      value <<= 3;

      value |= x[j] * 4 + y[j] * 2 + z[j] * 1;
    }

    return value;
  }


  static bits_type encode(bool x, bool y, bool z, std::size_t level) {
    bits_type result = 0;
    result[2] = x;
    result[1] = y;
    result[0] = z;
    
    return result << (3 * level);
  }
  


  
  cell(const coord& x, const coord& y, const coord& z)
    : bits( encode(x, y, z) ) {
    
  }

  cell(bool x, bool y, bool z, std::size_t level)
    : bits( encode(x, y, z, level) ) {
    
  }

  
  template<class Func>
  static void iter(T c, T max, Func&& func) {
    if(c) func(-1);
    func(0);
    if(c < max) func(1);
  }
  

  template<class Func>
  void neighbors(std::size_t level, Func&& func) const {
    const T max = 1ul << (max_level - level);
    
    decode([&](coord x, coord y, coord z) {
        const auto cx = (x >> level).to_ulong();
        const auto cy = (y >> level).to_ulong();
        const auto cz = (z >> level).to_ulong();

        iter(cx, max, [&](int dx) {
            auto rx = (cx + dx) << level;
            auto ax = std::abs(dx);
            iter(cy, max, [&](int dy) {
                auto ry = (cy + dy) << level;
                auto ay = std::abs(dy);
                iter(cz, max, [&](int dz) {
                    auto rz = (cz + dz) << level;
                    auto az = std::abs(dz);
                    if(ax + ay + az) {
                      func(rx, ry, rz);
                    }
                  });
              });
          });
      });
  }

  // cell iterator
  struct iterator {
    T value;
    T incr;

    cell operator*() const { return cell(value); }
    iterator& operator++() {
      value += incr;
      return *this;
    }

    bool operator!=(const iterator& other) const {
      return value != other.value;
    }
  };

  // subcell iterable
  struct children {
    const T origin;
    const T incr;
    
    children(const cell<T>& origin, std::size_t level)
      : origin(origin.bits.to_ulong()),
        incr(1ul << (3 * level)) { }

    iterator begin() const { return {origin, incr}; }
    iterator end() const { return {origin + (incr << 3), incr}; }    
  };

  explicit cell(const T& value) : bits(value) {  }
  
  cell next(std::size_t level) const {
    const T value = bits.to_ulong() + (1ul << (3 * level));
    return cell(value);
  }

  friend std::ostream& operator<<(std::ostream& out, const cell& self) {
    self.decode([&](coord x, coord y, coord z) {
        out << "(" << x << ", " << y << ", " << z << ")";
      });
    return out;
  }
  
};


template<class T>
using coord = typename cell<T>::coord;


using real = double;
using vec3 = Eigen::Matrix<real, 3, 1>;

struct debug {
  static std::size_t& level() {
    static std::size_t value = 0;
    return value;
  }
  
  std::size_t indent = 0;
  debug() : indent(level()++) { }
  ~debug() { --level(); }

  std::ostream& stream() const {
    return std::clog << std::string(indent, ' ');
  }
};


// brute force
template<class Distance, class Iterator>
static Iterator find_nearest(const Distance& distance, real& best, Iterator first, Iterator last) noexcept {
  Iterator res = last;

  for(Iterator it = first; it != last; ++it) {
    const real d = distance(*it);
    if(d < best) {
      best = d;
      res = it;
    }
  }

  return res;
}


// octree based
template<class Distance, class Iterator, class T>
static Iterator find_nearest(const Distance& distance, real& best, Iterator first, Iterator last,
                             const cell<T>& origin, std::size_t level = cell<T>::max_level) noexcept {
  // debug dbg;
  const std::size_t size = last - first;
  
  // base case
  if( (level == 0) || size <= cell<T>::brute_force_threshold) {
    return find_nearest(distance, best, first, last);
  }

  // recursive case: split cell
  Iterator result = last;

  // TODO start with cell containing query point if possible?
  for(cell<T> c : typename cell<T>::children(origin, level - 1)) {
    Iterator begin = std::lower_bound(first, last, c);
    if(begin == last) continue; // no point found in subcell

    // note: we want upper bound since we may have duplicate cells in the
    // range (e.g. many points in same leafs)
    Iterator end = std::lower_bound(begin, last, c.next(level - 1));
    
    // don't go down the octree unless it's worth it
    if(distance(c, level - 1) < best) {
      const real old_best = best;
      Iterator sub = find_nearest(distance, best, begin, end, c, level - 1);

      if(best < old_best) {
        result = sub;
      }
    }
  }

  return result;
};






template<class T>
class octree {
  struct item; 
  using data_type = std::vector< item >;
  
  data_type data;

  struct distance;
  
 public:

  void reserve(std::size_t count) { data.reserve(count); }
  
  static cell<T> hash(const vec3& p) {
    auto s = (p * cell<T>::resolution()).template cast<T>();
    return cell<T>(s.x(), s.y(), s.z());
  }

  static vec3 origin(const cell<T>& c) {
    vec3 res;
    c.decode([&](coord<T> x, coord<T> y, coord<T> z) {
        res = {x.to_ulong(), y.to_ulong(), z.to_ulong()};
      });

    return res / cell<T>::resolution();
  }
  
  
  void push(const vec3& p) {
    data.emplace_back(hash(p), p);
  }
  

  void sort() { std::sort(data.begin(), data.end()); }

  std::size_t size() const { return data.size(); }
  
  const vec3* nearest(const vec3& query) const {
    if(data.empty()) return nullptr;

    real best = std::numeric_limits<real>::max();
    auto it = find_nearest(distance{query}, best, data.begin(), data.end(), cell<T>(0));
    assert(it != data.end());
    
    return &it->p;
  }


  const vec3* brute_force(const vec3& query) const {
    if(data.empty()) return nullptr;
    
    real best = std::numeric_limits<real>::max();
    auto it = find_nearest(distance{query}, best, data.begin(), data.end());
    assert(it != data.end());
    
    return &it->p;
  }
  
};

template<class T>
struct octree<T>::item {
  cell<T> c;
  vec3 p;

  item(const cell<T>& c, const vec3& p) : c(c), p(p) { }

  friend bool operator<(const cell<T>& c, const item& self) {
    return c.bits.to_ulong() < self.c.bits.to_ulong();
  }

  friend bool operator<(const item& self, const cell<T>& c) {
    return self.c.bits.to_ulong() < c.bits.to_ulong();
  }

  friend bool operator<(const item& lhs, const item& rhs) {
    return lhs.c.bits.to_ulong() < rhs.c.bits.to_ulong();
  }
  
  
};


template<class T>
struct octree<T>::distance {
  const vec3 query;
  const vec3 scaled_query;

  mutable std::size_t point_count = 0, box_count = 0;

  ~distance() {
    std::clog << " point distances: " << point_count
              << " box distances: " << box_count << std::endl;
  }

  
  distance(const vec3& query)
    : query(query),
      scaled_query(query * cell<T>::resolution() ) { }


  
  // distance to point
  real operator()(const item& i) const {
    ++point_count;
    return (query - i.p).squaredNorm(); // TODO optimize with squaredNorm();
  }

  vec3 project(const cell<T>& c, std::size_t level) const {
    // compute scaled_query - proj(scaled_query) onto cell c at given level
    vec3 res;
    c.decode([&](coord<T> sx, coord<T> sy, coord<T> sz) {
        const vec3 low{sx.to_ulong(), sy.to_ulong(), sz.to_ulong()};
        const vec3 local = scaled_query - low;
        res = local.array() - local.array().min(1ul << level).max(0);
      });
    return res;
  }
  
  // distance to cell
  real operator()(const cell<T>& c, std::size_t level) const {
    ++box_count;
    return project(c, level).squaredNorm() / (cell<T>::resolution() * cell<T>::resolution());
  }
};





#endif
