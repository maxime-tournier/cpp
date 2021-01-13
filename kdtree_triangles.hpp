#ifndef KDTREE_TRIANGLES_HPP
#define KDTREE_TRIANGLES_HPP

#include <vector>
#include <algorithm>


template<class Real, class Index, std::size_t N=3>
struct kd_tree {
    
    using index_type = Index;
    using real_type = Real;

    struct vertex {
        real_type* coords;
    };

    struct triangle {
        index_type* indices;
        std::size_t counter;
    };

    class plane {
        // TODO by value?
        real_type* origin;
        std::size_t axis;
    public:
        plane(real_type* origin,
              std::size_t axis):
            origin(origin),
            axis(axis) { }
        
        bool positive(real_type* point) const {
            return point[axis] >= origin[axis];
        }
        
    };

    template<class Iterator>
    static Iterator split_positions(Iterator first, Iterator last, std::size_t axis) {
        const std::size_t size = last - first;
        if(!size) {
            return first;
        }
        
        std::sort(first, last, [axis](const vertex& lhs, const vertex& rhs) {
            return lhs[axis] < rhs[axis];
        });

        const std::size_t mid = size / 2;
        return first + mid;
    }


    template<class Iterator>
    static std::pair<Iterator, Iterator> split_triangles(Iterator first, Iterator last,
                                                         real_type* points,
                                                         plane p) {
        // TODO single sweep?
        
        // compute positive vertices
        for(Iterator it = first; it != last; ++it) {
            it->counter = 0;
            for(std::size_t i = 0; i < N; ++i) {
                if(p.positive(points[3 * (it->indices[i])])) {
                    ++it->counter;
                }
            }
        }

        // partition
        const Iterator negative = std::partition(first, last, [](const triangle& self) {
            return self.counter > 0;
        });

        const Iterator positive = std::partition(negative, last, [](const triangle& self) {
            return self.counter < 3;
        });

        return std::make_pair(negative, positive);
    }


    template<class TriangleIterator>
    static void create(TriangleIterator first_triangle, TriangleIterator last_triangle,
                       real_type* points,
                       std::size_t axis) {
        const auto pivot = split_positions(first_point, last_point, axis);
        const plane p = {pivot->coords, axis};
        
        const auto subsets = split_triangles(first_triangle, last_triangle, points, axis);

        const std::size_t next = ++axis % N;
        create(first_triangle, subsets.first, next);
        create(subsets.first, subsets.second, next);
        create(subsets.second, last_triangle, next);
        
    }
    
    
    static void create(real_type* points, std::size_t point_size,
                       index_type* triangles, std::size_t triangle_size) {
        // build vertex array
        std::vector<vertex> vertices(point_size);
        for(std::size_t i = 0; i < point_size; ++i) {
            vertices[i].coords = points + (i * N);
        }

        // build triangle array
        std::vector<triangle> triangles(triangle_size);
        for(std::size_t i = 0; i < triangle_size; ++i) {
            triangle[i].indices = triangles + (i * 3);
        }

        // 
        
    }
    
    

};

#endif
