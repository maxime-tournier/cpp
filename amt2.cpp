// -*- compile-command: "CXXFLAGS=-std=c++14 make amt2" -*-

#ifndef AMT_HPP
#define AMT_HPP

#include "sparse_array.hpp"
#include <array>
#include <utility>

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
  using children_type = sparse::base<child_node>;
  sparse::ref<children_type> children;

  using single_type = sparse::derived<child_node, 1>;
  static sparse::ref<children_type> make_single(std::size_t index, const T& value) {
    const std::size_t sub = index >> shift;
    const std::size_t rest = index & mask;
    return std::make_shared<single_type>(1ul << sub, child_node(rest, value));
  }
  
  node(std::size_t index, const T& value): children(make_single(index, value)) { }
  
  node(sparse::ref<children_type> children):
    children(std::move(children)) { }
  
  
  const T& get(std::size_t index) const {
    const std::size_t sub = index >> shift;
    const std::size_t rest = index & mask;

    return children->get(sub).get(rest);
  }

  node set(std::size_t index, const T& value) const {
    const std::size_t sub = index >> shift;
    const std::size_t rest = index & mask;

    return children->template set<max_size>(sub, children->contains(sub) ?
                                            children->get(sub).set(rest, value) :
                                            child_node(rest, value));
  }
  
};


template<class T, std::size_t B, std::size_t L>
struct node<T, 0, B, L> {
  static constexpr std::size_t max_size = 1ul << L;
  
  using children_type = sparse::base<T>;
  sparse::ref<children_type> children;

  using single_type = sparse::derived<T, 1>;
  node(std::size_t index, const T& value):
    children(std::make_shared<single_type>(1ul << index, value)) { }

  node(sparse::ref<children_type> children): children(std::move(children)) { }
  
  const T& get(std::size_t index) const {
    return children->get(index);
  }

  node set(std::size_t index, const T& value) const {
    return {children->template set<max_size>(index, value)};
  }
  
};



template<class T, std::size_t B, std::size_t L>
class array {
  static constexpr std::size_t index_bits = sizeof(std::size_t) * 8;
  static constexpr std::size_t max_level = (index_bits - L) / B;

  using root_type = node<T, max_level, B, L>;
  root_type root;

public:
  array(root_type root={{}}): root(root) { }
  
  const T& get(std::size_t index) const {
    return root.get(index);
  }
  
  array set(std::size_t index, const T& value) const {
    // TODO branch prediction should work here
    if(root.children) {
      return root.set(index, value);
    }

    return root_type(index, value);
  }
  
};



#endif


#include <iostream>

int main(int, char**) {
  array<double, 8, 8> bob;
  bob = bob.set(0, 1.0);
  
  std::clog << bob.get(0) << std::endl;
  
  return 0;
}
