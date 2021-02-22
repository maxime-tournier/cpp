#ifndef KDTREE_TRIANGLES_HPP
#define KDTREE_TRIANGLES_HPP

#include <algorithm>
#include <vector>
#include <memory>

#include "point_triangle.hpp"

#include <Eigen/Core>

template<class Real, class Index, std::size_t N = 3>
struct kd_tree {
  using index_type = Index;
  using real_type = Real;

  struct vertex {
    const real_type* coords;
  };

  struct triangle {
    const index_type* indices;
    std::size_t counter = 0;

    // real_type distance2(vertex query, const vertex* data) const {
    //   using vec3 = Eigen::Matrix<real_type, 3, 1>;
    //   using mat3x2 = Eigen::Matrix<real_type, 3, 2>;      

    //   const auto q = vec3::Map(query.coords);
    //   const auto p = {vec3::Map(data + indices[0]),
    //                   vec3::Map(data + indices[1]),
    //                   vec3::Map(data + indices[2])};
    //   mat3x2 A;
    //   A << (p[1] - p[0]), (p[2] - p[0]);

    //   const vec3 n = A.col(0).cross(A.col(1));
    //   const vec3 s = q - p[0];
      
    //   const vec3 b = s - (n * n.dot(s)) / n.squaredNorm();
      
    // }
    // TODO point-triangle distance2
  };

  class plane {
    vertex origin;
    std::size_t axis;
  public:
    plane(vertex origin, std::size_t axis):
      origin(origin), axis(axis) {}
    
    bool positive(vertex v) const {
      return v.coord[axis] >= origin.coord[axis];
    }

    real_type distance2(vertex v) const {
      const real_type delta = v.coord[axis] - origin.coord[axis];
      // TODO distance?
      return delta * delta;
    }
  };


  // partition(3) triangle range according to given plane
  static std::pair<triangle*, triangle*>
  split_triangles(triangle* first, triangle* last, const vertex* vertices,
                  plane p) {
    // compute positive vertices
    for(auto it = first; it != last; ++it) {
      it->counter = 0;
      for(std::size_t i = 0; i < N; ++i) {
        if(p.positive(vertices[(it->indices[i])])) {
          ++it->counter;
        }
      }
    }

    // partition
    const auto negative = std::partition(
        first, last, [](const triangle& self) { return self.counter > 0; });

    const auto positive = std::partition(
        negative, last, [](const triangle& self) { return self.counter < 3; });

    return std::make_pair(negative, positive);
  }

  template<class T>
  using ref = std::shared_ptr<T>;

  struct node {
    const plane p;

    const triangle* first;
    const triangle* last;
    
    const ref<node> negative, overlap, positive;
    node(plane p, const triangle* first, const triangle* last,
         ref<node> negative, ref<node> overlap, ref<node> positive):
        p(p),
        first(first),
        last(last),
        negative(negative),
        overlap(overlap),
        positive(positive) {
      assert(last > first);
    }


    struct closest_type {
      const triangle* tri = nullptr;
      real_type distance2 = std::numeric_limits<real_type>::infinity();
      // TODO store proj as well?

      closest_type min(closest_type other) const {
        if(distance2 < other.distance2) {
          return *this;
        } else {
          return other;
        }
      }
    };


    static closest_type closest_brute_force(vertex query, const triangle* first,
                                            const triangle* last, real_type) {
      // TODO exploit best?
      closest_type res;
      for(auto it = first; it != last; ++it) {
        const real_type distance2 = it->distance2(query);
        if(distance2 < res.distance2) {
          res.distance2 = distance2;
          res.tri = *it;
        }
      }

      return res;
    }


    closest_type closest(vertex query, const ref<node>& near, const ref<node>& far, real_type best) const {
      closest_type res;      
      for(auto sub: {near, overlap}) {
        if(sub) {
          res = res.min(sub->closest(query, best));
          if(res.distance2 < best) {
            best = res.distance2;
          }
        }
      }
      
      if(p.distance2(query) < best) {
        res = res.min(far->closest(query, best));
      }

      return res;
    }

    static constexpr std::size_t brute_force_threshold = 1;
    
    closest_type closest(vertex query,
                         real_type best=std::numeric_limits<real_type>::infinity()) const {
      if(last - first < brute_force_threshold) {
        return closest_brute_force(query, first, last, best);
      }
      
      closest_type res;
      if(p.positive(query)) {
        return closest(query, positive, negative, best);
      } else {
        return closest(query, negative, positive, best);
      }
    }
    
  };

  static ref<node> create(triangle* first,
                          triangle* last,
                          const vertex* vertex_data, std::size_t axis) {
    // base case
    if(first == last) {
      return {};
    }
    
    // 1. fetch triangle vertices
    std::vector<vertex> vertices;
    for(auto it = first; it != last; ++it) {
      for(std::size_t i = 0; i < 3; ++i) {
        vertices.emplace_back(vertex_data[i]);
      }
    }

    // 2. sort vertices along axis
    const auto first_vertex = vertices.begin();
    auto last_vertex = vertices.end();
    std::sort(first_vertex, last_vertex, [axis](vertex lhs, vertex rhs) {
      return lhs.coords[axis] < rhs.coords[axis];
    });

    // 3. prune duplicates
    last_vertex = std::unique(first_vertex, last_vertex);

    // 4. find pivot
    const auto pivot = (first_vertex + last_vertex) / 2;
    const plane p = {pivot->coords, axis};

    // 5. partition triangles
    const auto subsets =
        split_triangles(first, last, vertex_data, axis);

    // 7. recurse on subclasses
    const std::size_t next = ++axis % N;
    const auto negative = create(first, subsets.first, vertex_data, next);
    const auto overlap = create(subsets.first, subsets.second, vertex_data, next);
    const auto positive = create(subsets.second, last, vertex_data, next);

    // 8. node
    return std::make_shared<node>(p, first, last, negative, overlap, positive);
  }

  
  struct result {
    ref<node> root;
    std::vector<vertex> vertices;
    std::vector<triangle> triangles;

  };

  static void create(result& out,
                     const real_type* point_data, std::size_t point_size,
                     const index_type* triangle_data, std::size_t triangle_size) {
    // build vertex array
    out.vertices.resize(point_size);
    for(std::size_t i = 0; i < point_size; ++i) {
      out.vertices[i].coords = point_data + (i * N);
    }

    // build triangle array
    out.triangles.resize(triangle_size);
    for(std::size_t i = 0; i < triangle_size; ++i) {
      out.triangles[i].indices = triangle_data + (i * 3);
    }
    
    // create
    const std::size_t axis = 0;
    return create(out.triangles.data(), out.triangles.data() + triangle_size,
                  out.vertices.data(), axis);
  }

  
  
};

#endif
