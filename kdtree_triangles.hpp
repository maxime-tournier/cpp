#ifndef KDTREE_TRIANGLES_HPP
#define KDTREE_TRIANGLES_HPP

#include <algorithm>
#include <vector>
#include <memory>

template<class Real, class Index, std::size_t N = 3>
struct kd_tree {
  using index_type = Index;
  using real_type = Real;

  struct vertex {
    const real_type* coords;
  };

  struct triangle {
    const index_type* indices;
    std::size_t counter;
  };

  class plane {
    vertex origin;
    std::size_t axis;
  public:
    plane(vertex origin, std::size_t axis):
      origin(origin), axis(axis) {}
    
    bool positive(vertex v) const {
      return v.coord[axis] >= v.coord[axis];
    }
  };


  // partition(3) triangle range according to given plane
  template<class Iterator>
  static std::pair<Iterator, Iterator>
  split_triangles(Iterator first, Iterator last, const vertex* vertices,
                  plane p) {
    // compute positive vertices
    for(Iterator it = first; it != last; ++it) {
      it->counter = 0;
      for(std::size_t i = 0; i < N; ++i) {
        if(p.positive(vertices[(it->indices[i])])) {
          ++it->counter;
        }
      }
    }

    // partition
    const Iterator negative = std::partition(
        first, last, [](const triangle& self) { return self.counter > 0; });

    const Iterator positive = std::partition(
        negative, last, [](const triangle& self) { return self.counter < 3; });

    return std::make_pair(negative, positive);
  }

  template<class T>
  using ref = std::shared_ptr<T>;

  struct node {
    const plane p;
    const ref<node> negative, overlap, positive;
    node(plane p, ref<node> negative, ref<node> overlap, ref<node> positive):
        p(p), negative(negative), overlap(overlap), positive(positive) {}
  };

  template<class TriangleIterator, class VertexIterator>
  static ref<node> create(TriangleIterator first_triangle,
                          TriangleIterator last_triangle,
                          const vertex* vertex_data, std::size_t axis) {
    // base case
    if(first_triangle == last_triangle) {
      return {};
    }
    
    // 1. fetch triangle vertices
    std::vector<vertex> vertices;
    for(auto it = first_triangle; it != last_triangle; ++it) {
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
        split_triangles(first_triangle, last_triangle, vertex_data, axis);

    // 7. recurse
    const std::size_t next = ++axis % N;
    const auto negative = create(first_triangle, subsets.first, vertex_data, next);
    const auto overlap = create(subsets.first, subsets.second, vertex_data, next);
    const auto positive = create(subsets.second, last_triangle, vertex_data, next);
    
    return std::make_shared<node>(p, negative, overlap, positive);
  }

  struct result {
    ref<node> root;
    std::vector<vertex> vertices;
    std::vector<triangle> triangles;
  };

  static void create(result& out,
                     real_type* point_data, std::size_t point_size,
                     index_type* triangle_data, std::size_t triangle_size) {
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
    return create(out.triangles.begin(), out.triangles.end(), out.vertices.data(), axis);
  }
  
};

#endif
