#ifndef CPP_OCTREE_HPP
#define CPP_OCTREE_HPP

#include <vector>
#include <bitset>

#include <algorithm>

#include <iostream>

#include <Eigen/Core>

namespace detail {
  
// a fast linear (morton) octree
template<class T>
struct cell {

  static constexpr std::size_t brute_force_threshold = 256;
  
  static constexpr std::size_t max_level = (8 * sizeof(T)) / 3;
  using coord = std::bitset<max_level>;
  
  static constexpr T resolution() { return 1ul << max_level; }

  static constexpr std::size_t size = 3 * max_level;
  using bits_type = std::bitset<size>;
  bits_type bits;

  explicit cell(const bits_type& bits={}) noexcept : bits(bits) { }
    
  // decode a cell code as individual coordinate codes
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

  // encode cell from x y z codes
  static cell encode(const coord& x, const coord& y, const coord& z) {
    T value = 0;
    for(std::size_t i = 0; i < max_level; ++i) {
      const std::size_t j = max_level - 1 - i;
      value <<= 3;

      value |= x[j] * 4 + y[j] * 2 + z[j] * 1;
    }

    return cell(value);
  }


  // encode a cell at a given level
  static cell encode(bool x, bool y, bool z, std::size_t level) {
    bits_type result = 0;
    result[2] = x;
    result[1] = y;
    result[0] = z;
    
    return cell(result << (3 * level));
  }
  

  // generate possible delta (-1, 0, 1) for a cell given maximum value
  template<class Func>
  static void iter_delta(T c, T max, const Func& func) {
    if(c) func(-1);
    func(0);
    if(c < max) func(1);
  }
  

  // iterate cell neighbors at given level (cell must be admissible at this
  // level i.e. zero lower-order bits)
  template<class Func>
  void neighbors(std::size_t level, Func&& func) const {
    const T max = 1ul << (max_level - level);
    
    decode([&](coord x, coord y, coord z) {
        const auto cx = (x >> level).to_ulong();
        const auto cy = (y >> level).to_ulong();
        const auto cz = (z >> level).to_ulong();

        iter_delta(cx, max, [&](int dx) {
            const auto rx = (cx + dx) << level;
            const auto ax = std::abs(dx);
                        
            iter_delta(cy, max, [&](int dy) {
                const auto ry = (cy + dy) << level;
                const auto ay = std::abs(dy);
                                
                iter_delta(cz, max, [&](int dz) {
                    const auto rz = (cz + dz) << level;
                    const auto az = std::abs(dz);
                    if(ax + ay + az) {
                      func(rx, ry, rz);
                    }
                  });
              });
          });
      });
  }


  template<class Func>
  void children(std::size_t level, const Func& func) const noexcept {
    const T first = bits.to_ulong();
    const T incr = 1ul << (3 * level);
    const T last = first + (incr << 3);

    for(T value = first, next; value != last; value = next) {
      next = value + incr;
      func(cell(value), cell(next));
    }
  }
    
  // debugging
  friend std::ostream& operator<<(std::ostream& out, const cell& self) {
    self.decode([&](coord x, coord y, coord z) {
        out << "(" << x << ", " << y << ", " << z << ")";
      });
    return out;
  }
    
};


template<class T>
using coord = typename cell<T>::coord;

// a fixed-size (dynamically chosen) sorted array
template<class Key, class Value>
class sorted_array {
    
  struct item_type {
    Key key;
    Value value;
    
    bool operator<(const item_type& other) const { return key < other.key; }
    
    friend bool operator<(const item_type& self, const Key& key) { return self.key < key; }
    friend bool operator<(const Key& key, const item_type& self) { return key < self.key; }    
  };
  
  std::vector<item_type> items;
public:
  sorted_array(std::size_t size, const Key& key = {}, const Value& value = {})
    : items(size, item_type{key, value}) { };

  std::size_t size() const { return items.size(); }
  
  void insert(const Key& key, const Value& value) {
    items.pop_back();

    auto it = std::upper_bound(items.begin(), items.end(), key);
    items.emplace(it, key, value);
  }

  auto begin() const -> decltype(items.begin()) { return items.begin(); }
  auto end() const -> decltype(items.begin()) { return items.begin(); }

  auto back() const -> decltype(items.back()) { return items.back(); }    
};


// brute force 1-nn
template<class Real, class Distance, class Iterator>
static Iterator find_nearest(const Distance& distance, Real& best, Iterator first, Iterator last) noexcept {
  Iterator res = last;

  for(Iterator it = first; it != last; ++it) {
    const Real d = distance(*it);
    if(d < best) {
      best = d;
      res = it;
    }
  }
    
  return res;
}


// brute force k-nn
template<class Real, class Distance, class Iterator>
static void find_nearest(sorted_array<Real, Iterator>& result, const Distance& distance,
                         Iterator first, Iterator last) noexcept {
  for(Iterator it = first; it != last; ++it) {
    const Real d = distance(*it);
    if(d < result.back().key) {
      result.insert(d, it);
    }
  }
}



// octree based 1-nn
template<class Real, class Distance, class Iterator, class T>
static Iterator find_nearest(const Distance& distance, Real& best, Iterator first, Iterator last,
                             const cell<T>& origin, std::size_t level = cell<T>::max_level) noexcept {
  const std::size_t size = last - first;
  
  // base case
  if(size <= cell<T>::brute_force_threshold || !level) {
    return find_nearest(distance, best, first, last);
  }

  // recursive case: split cell

  const std::size_t next_level = level - 1;
  
  struct chunk_type {
    Real d;
    cell<T> c;
    Iterator begin, end;
    
    inline bool operator<(const chunk_type& other) const { return d < other.d; }
  };

  chunk_type chunks[8];
  std::size_t count = 0;

  origin.children(next_level, [&](cell<T> curr, cell<T> next) noexcept {
      const Iterator begin = std::lower_bound(first, last, curr);
      if(begin == last) return; // no point found in subcell
            
      // note: we want upper bound since we may have duplicate cells in the
      // range (e.g. many points in same leafs)
      const Iterator end = std::lower_bound(begin, last, next);
            
      // const cell<T> child{curr};
      chunks[count++] = {distance(curr, next_level), curr, begin, end};
    });
    
  assert(count <= 8);

  std::sort(chunks, chunks + count);

  Iterator result = last;
    
  for(auto it = chunks, end = chunks + count; it != end; ++it) {
    if(it->d < best) {
      const Real old_best = best;
      const Iterator sub = find_nearest(distance, best, it->begin, it->end, it->c, next_level);
        
      if(best < old_best) {
        result = sub;
      }

    }
  }

  return result;
};


// octree based k-nn
template<class Real, class Distance, class Iterator, class T>
static void find_nearest(sorted_array<Real, Iterator>& result, const Distance& distance,
                         Iterator first, Iterator last,
                         const cell<T>& origin, std::size_t level = cell<T>::max_level) noexcept {
  // range size
  const std::size_t size = last - first;
  
  // base case
  if( (level == 0) || size <= cell<T>::brute_force_threshold) {
    find_nearest(result, distance, first, last);
  }
  
  // recursive case: split cell
  const std::size_t next_level = level - 1;

  // subcell info TODO rename
  struct chunk_type {
    Real d;
    cell<T> c;
    Iterator begin, end;
    
    bool operator<(const chunk_type& other) const { return d < other.d; }
  };

  chunk_type chunks[8];
  std::size_t count = 0;

  // obtain subcell info, skip if empty
  for(cell<T> c : typename cell<T>::children(origin, next_level)) {
    const Iterator begin = std::lower_bound(first, last, c);
    if(begin == last) continue; // no point found in subcell

    // note: we want upper bound since we may have duplicate cells in the
    // range (e.g. many points in same leafs)
    const Iterator end = std::lower_bound(begin, last, c.next(next_level));
    
    chunks[count] = {distance(c, next_level), c, begin, end};
    ++count; 
  }

  assert(count <= 8);

  // process subcells by distance to query point
  std::sort(chunks, chunks + count);
  
  for(auto it = chunks, end = chunks + count; it != end; ++it) {
    // don't visit subcell if our furthest guess is closer than the subcell
    if(it->d < result.back().key) {
      find_nearest(result, distance, it->begin, it->end, it->c, next_level);
    }
  }

  return result;
};

}


template<class Real=double, class T=unsigned long>
class octree {
  struct item; 
    
  using data_type = std::vector<item>;
  data_type data;

public:
  struct distance;
    
  // TODO use traits
  using real = Real;
  using vec3 = Eigen::Matrix<real, 3, 1>;
    

  using cell = detail::cell<T>;
  using coord = detail::coord<T>;
    
  // preallocate octree data
  void reserve(std::size_t count) { data.reserve(count); }

  // clear octree data
  void clear() { data.clear(); }

  // number of octree cells
  std::size_t size() const { return data.size(); }
  
  static cell hash(const vec3& p) {
    auto s = (p * cell::resolution()).template cast<T>();
    return cell::encode(s.x(), s.y(), s.z());
  }

  static vec3 origin(const cell& c) {
    vec3 res;
    c.decode([&](const coord& x, const coord& y, const coord& z) {
        res = {x.to_ulong(), y.to_ulong(), z.to_ulong()};
      });
        
    return res / cell::resolution();
  }
  

  // append a point to the octree
  void add(const vec3* p) {
    data.push_back({hash(*p), p});
  }

  // sort octree cells
  void sort() { std::sort(data.begin(), data.end()); }
    
  // nearest-neighbor search
  const vec3* nearest(const vec3& query) const {
    if(data.empty()) return nullptr;
        
    real best = std::numeric_limits<real>::max();
    auto it = find_nearest(distance{query}, best, data.begin(), data.end(), cell(0));
    assert(it != data.end());
        
    return it->p;
  }

  template<class OutputIterator>
  void nearest(OutputIterator out, const vec3& query, std::size_t count = 1) const {
    assert(count >= data.size());
        
    using iterator = typename data_type::iterator;
    
    detail::sorted_array<real, iterator> knn(count, std::numeric_limits<real>::max());
    find_nearest(knn, distance{query}, data.begin(), data.end(), cell(0));

    for(auto& it : knn) {
      *out++ = it.value.p;
    }
  }
  
  
};

template<class Real, class T>
struct octree<Real, T>::item {
  cell c;
  const vec3* p;

  friend bool operator<(const T& c, const item& self) {
    return c < self.c.bits.to_ulong();
  }

  friend bool operator<(const item& self, const T& c) {
    return self.c.bits.to_ulong() < c;
  }

  friend bool operator<(const cell& c, const item& self) {
    return c.bits.to_ulong() < self.c.bits.to_ulong();
  }

  friend bool operator<(const item& self, const cell& c) {
    return self.c.bits.to_ulong() < c.bits.to_ulong();
  }

  
  friend bool operator<(const item& lhs, const item& rhs) {
    return lhs.c.bits.to_ulong() < rhs.c.bits.to_ulong();
  }
};


template<class Real, class T>
struct octree<Real, T>::distance {
  const vec3 query;
  const vec3 scaled_query;

  // mutable std::size_t point_count = 0, box_count = 0;

  // ~distance() {
  //   std::clog << " point distances: " << point_count
  //             << " box distances: " << box_count << std::endl;
  // }
    
  distance(const vec3& query)
    : query(query),
      scaled_query(query * cell::resolution()) { }
    
  // distance to point
  real operator()(const vec3& p) const noexcept {
    // ++point_count;
    return (query - p).squaredNorm();
  }

  real operator()(const item& i) const noexcept {
    // ++point_count;
    return operator()(*i.p);
  }


  // compute scaled_query - proj(scaled_query) onto cell c at given level    
  vec3 project(const cell& c, std::size_t level) const {
    vec3 res;
    c.decode([&](coord sx, coord sy, coord sz) {
        const vec3 low{sx.to_ulong(), sy.to_ulong(), sz.to_ulong()};
        const vec3 local = scaled_query - low;
        res = local.array() - local.array().min(1ul << level).max(0);
      });
    return res;
  }

  // distance to cell
  real operator()(const cell& c, std::size_t level) const noexcept {
    // ++box_count;
    return project(c, level).squaredNorm()  / (cell::resolution() * cell::resolution());
  }
};





#endif
