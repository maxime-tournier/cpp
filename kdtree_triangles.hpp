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
        std::size_t negative_triangles = 0;
        std::size_t positive_triangles = 0;
        
        for(Iterator it = first; it != last; ++it) {
            std::size_t positive_points = 0;
            for(std::size_t i = 0; i < N; ++i) {
                if(p.positive(points[3 * (it->indices[i])])) {
                    ++positive_points;
                }
            }
            switch(positive_points) {
            case 0: {
                // negative: put it with negatives
                std::swap(*it, *(first + negative_triangles++));
                break;
            }
            case 3: {
                // positive: put it with positives
                std::swap(*it, *(first + negative_triangles++));
                break;
            }
            default:
            }
        }
    }

    static void create(real_type* points, std::size_t point_size,
                       index_type* triangles, std::size_t triangle_size) {
        // build vertex array
        std::vector<vertex> vertices(point_size);
        for(std::size_t i = 0; i < point_size; ++i) {
            vertices[i].coords = points + (i * N);
        }

        // build triangle array
        
    }
    
    

};

#endif
