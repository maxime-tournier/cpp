#ifndef CPP_KDTREE_HPP
#define CPP_KDTREE_HPP

using real = double;
using vec3 = Eigen::Matrix<real, 3, 1>;

class kdtree {

  using index_type = std::size_t;
  using dir_type = unsigned char;
  
  struct node {
    dir_type dir;
    index_type left, right;
  };

  std::vector<node> tree;

  using storage_type = std::vector<std::pair<real, index_type>>;
  
  template<class Iterator>
  index_type build_impl(Iterator first, Iterator last, dir_type dir, const vec3* positions, storage_type& storage) {
    const std::size_t size = last - first; assert(size);

    if(size == 1) {
      const index_type index = *first;
      tree[index].left = index;
      tree[index].right = index;
      tree[index].splitdir = direction;
      return index;
    }

    // map of position[i][direction] -> i
    storage.clear();

    for(iterator_type it = first; it != last; ++it) {
      // TODO optimize cache miss here by storing positions in the roi order
      storage.emplace_back(positions[*it][direction], *it);
    }

    std::sort(storage.begin(), storage.end());
    
    // replace list *contents* by the indices sorted by their position 
    Iterator it = first;
    for(auto sit = storage.begin(), last = storage.end(); sit != last; ++sit) {
      *it = sit->second; ++it;
    }
    
    // split list in 2
    it = first + size / 2;
    
    // add node for split point
    const index_type index = *it;
    
    tree[index].splitdir = direction;
    tree[index].left = index;
    tree[index].right = index;
    
    // split children recursively along next direction
    dir_type new_dir = direction + 1; 
    if(new_dir == dim) new_dir = 0; // modulo dim
    
    // left subtree
    if(it - first > 0) {
      tree[index].left = build_impl(first, it, new_dir, positions, storage);
    }

    // right subtree, excluding split point
    ++it;
    if(last - it > 0) {
      tree[index].right = build_impl(it, last, new_dir, positions, storage);
    }
    
    // return child index to parent
    return index;
  }


  void closest_impl(const vec3& query, index_type index, const vec3* positions) const {
    assert(index < tree.size());
    const auto& node = tree[index];
    
    const unsigned splitdir = node.splitdir;
    assert(splitdir < x.size());
    
    // current node support point
    assert(index < positions.size());
    const Coord& pos = positions[index];
    
    using Real = typename Coord::value_type;

    // coordinates along split plane normal
    const Real cx = x[splitdir], cpos = pos[splitdir];

    const Real diff = cpos - cx;
    const Real diff2 = diff * diff;

    // our best squared distance so far
    const Real Dmin2 = res.first;

    bool visit_left = false, visit_right = false;
    
    // B(x, Dmin) intersect split plane
    if(diff2 <= Dmin2) {
        // update our best guess
        const Real d = (x - pos).norm2();
        if(d < Dmin2) {
            res.first = d;
            res.second = index;
        }
        
        // visit both sides
        visit_left = true;
        visit_right = true;
    } else {
        // (strictly) no intersection: no need to visit the other side
        visit_left = diff > 0;
        visit_right = diff < 0;
    }

    if(visit_left && (node.left != index)) closest_impl(res, x, node.left, positions, tree);
    if(visit_right && (node.right != index)) closest_impl(res, x, node.right, positions, tree);


  }
  
public:

  template<class Iterator>
  void build(Iterator first, Iterator last) {
    const index_type size = last - first;
    
    tree.resize(size);
    std::vector<index_type> source; source.reserve(size);
    
    for(index_type i = 0; i < size; ++i) {
      source.push_back(i);
    }

    // distance -> index map
    storage_type storage;
    storage.reserve(size);
    
    build_impl(source.begin(), source.end(), 0, &(*first), storage);
  }


  template<class Iterator>
  index_type closest(const vec3& query, Iterator first, Iterator last) const {
    
  }


};






#endif
