#ifndef SPARSE_AMT_HPP
#define SPARSE_AMT_HPP

#include "sparse.hpp"


namespace amt {
  
  template<class T, std::size_t ... Is, class Func>
  static constexpr std::array<T, sizeof...(Is)> unpack(std::index_sequence<Is...>,
                                                       const Func& func) {
    return {func(std::integral_constant<std::size_t, Is>{})...};
  }

  template<class T, std::size_t N, class Func>
  static constexpr std::array<T, N> unpack(const Func& func) {
    return unpack<T>(std::make_index_sequence<N>(), func);
  }


  template<class T, std::size_t level, std::size_t B, std::size_t L>
  struct node {
    static constexpr std::size_t max_size = 1ul << B;
    static constexpr std::size_t shift = L + B * (level - 1);
    static constexpr std::size_t mask = (1ul << (shift + 1)) - 1;
  
    using child_node = node<T, level - 1, B, L>;
    using children_type = sparse::array<child_node>;

    children_type children;

    static children_type make_single(std::size_t index, const T& value) {
      const std::size_t sub = index >> shift;
      const std::size_t rest = index & mask;

      return children_type().set(sub, child_node(rest, value));
    }
  
    node(std::size_t index, const T& value):
      children(make_single(index, value)) { }
  
    node(children_type children):
      children(std::move(children)) { }
  
  
    const T& get(std::size_t index) const {
      const std::size_t sub = index >> shift;
      const std::size_t rest = index & mask;

      assert(children.contains(sub));
      return children.get(sub).get(rest);
    }

    node set(std::size_t index, const T& value) const {
      const std::size_t sub = index >> shift;
      const std::size_t rest = index & mask;

      if(children.contains(sub)) {
        // TODO recursive call to set chokes compiler
        return children.set(sub, children.get(sub).set(rest, value));
      } else {
        return children.set(sub, child_node(rest, value));
      }
    }


    node emplace(std::size_t index, const T& value) {
      const std::size_t sub = index >> shift;
      const std::size_t rest = index & mask;

      if(children.contains(sub)) {
        auto& child = children.get(sub);
        child = child.emplace(rest, value);
        return std::move(children);
      }
    
      return children.set(sub, child_node(rest, value));
    }
  
  };


  template<class T, std::size_t B, std::size_t L>
  struct node<T, 0, B, L> {
    static constexpr std::size_t max_size = 1ul << L;
   
    using children_type = sparse::array<T>;
    children_type children;

    node(std::size_t index, const T& value):
      children(children_type().set(1ul << index, value)) { 
    }
  
    node(children_type children):
      children(std::move(children)) { }

    node(node&&) = default;
    node(const node&) = default;  

    node& operator=(node&&) = default;
    node& operator=(const node&) = default;  

  
    const T& get(std::size_t index) const {
      return children.get(index);
    }

    node set(std::size_t index, const T& value) const {
      return children.set(index, value);
    }

    node emplace(std::size_t index, const T& value) {
      if(children.contains(index)) {
        children.get(index) = value;
        return children;
      }

      return children.set(index, value);
    }
  
  };



  template<class T, std::size_t B, std::size_t L>
  class array {
    static constexpr std::size_t index_bits = sizeof(std::size_t) * 8;
    static constexpr std::size_t max_level = (index_bits - L) / B;

    using root_type = node<T, max_level, B, L>;
    root_type root;

  public:
    array(root_type root={{}}): root(std::move(root)) { }
  
    const T& get(std::size_t index) const {
      return root.get(index);
    }
  
    array set(std::size_t index, const T& value) const& {
                                                         return root.set(index, value);
    }

    array set(std::size_t index, const T& value) && {
                                                     return root.emplace(index, value);
    }

  
  };

}


#endif
