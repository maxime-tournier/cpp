// -*- compile-command: "c++ -std=c++14 compressed.cpp -o compressed -lstdc++ -g" -*-

#include "compressed.hpp"
#include <iostream>

template<class T>
using ref = std::shared_ptr<T>;


template<class T, std::size_t B, std::size_t L, std::size_t D>
struct node {
  static constexpr std::size_t max_size = 1ul << B;
  using children_type = ref<array<node<T, B, L, D - 1>>>;
  children_type children;
};

template<class T, std::size_t B, std::size_t L>
struct node<T, B, L, 0> {
  static constexpr std::size_t max_size = 1ul << L;
  using children_type = ref<array<T>>;
  children_type children;
};

template<std::size_t B, std::size_t L>
struct traits {
  static constexpr std::size_t bits = sizeof(std::size_t) * 8;
  static_assert(L <= bits, "size error");
  
  static constexpr std::size_t inner_levels = (bits - L) / B;  
  static constexpr std::size_t total_levels = 1 + inner_levels;
  static_assert(L + inner_levels * B <= bits, "size error");

  using level_indices = std::make_index_sequence<total_levels>;
  using index_array = std::array<std::size_t, total_levels>;

  template<std::size_t D>
  static constexpr std::size_t mask() {
    if(D) {
      return ((1ul << (B + 1)) - 1) << (L + (D - 1) * B);
    } else {
      return (1ul << (L + 1)) - 1;
    }
  }

  template<std::size_t D>
  static constexpr std::size_t offset() {
    if(D) {
      return L + (D - 1) * B;
    } else {
      return 0;
    }
  }

  template<std::size_t... Ds>
  static constexpr index_array make_masks(std::index_sequence<Ds...>) {
    return {mask<Ds>()...};
  }

  template<std::size_t... Ds>
  static constexpr index_array make_offsets(std::index_sequence<Ds...>) {
    return {offset<Ds>()...};
  }
  
  static constexpr index_array masks = make_masks(level_indices{});
  static constexpr index_array offsets = make_offsets(level_indices{});

  // split index
  template<std::size_t... Ds>
  static index_array split(std::size_t index,
                           std::index_sequence<Ds...>) {
    return {((index & masks[Ds]) >> offsets[Ds])...};
  }
  
};

template<std::size_t B, std::size_t L>
constexpr typename traits<B, L>::index_array traits<B, L>::masks;

template<std::size_t B, std::size_t L>
constexpr typename traits<B, L>::index_array traits<B, L>::offsets;




template<class T, std::size_t B, std::size_t L>
class hamt: public traits<B, L> {
  template<std::size_t D> using node_type = node<T, B, L, D>;

  // root node is toplevel
  using root_type = node_type<hamt::total_levels - 1>;
  root_type root;
  hamt(root_type root): root(std::move(root)) { }
  
  using index_array = typename hamt::traits::index_array;
  using level_indices = typename hamt::traits::level_indices;  
  
  // get
  static const T& get(const index_array& split, const node_type<0>& self) {
    return self.children->get(split[0]);
  }
  
  template<std::size_t D>
  static const T& get(const index_array& split, const node_type<D>& self) {
    return get(split, self.children->get(split[D]));
  }


  // make
  template<std::size_t D>
  static node_type<D> make(const index_array& split, T&& value) {
    using children_type = storage<node_type<D - 1>, (1ul << B), 1>;

    return {std::make_shared<children_type>(1ul << split[D],
                                            make<D - 1>(split, std::move(value)))};
  }
            
  template<>
  static node_type<0> make<0>(const index_array& split, T&& value) {
    using children_type = storage<T, (1ul << L), 1>;
    return {std::make_shared<children_type>(1ul << split[0], std::move(value))};
  };

  
  // set
  static node_type<0> set(const index_array& split, const node_type<0>& self, T&& value) {
    assert(self.children);
    if(self.children->has(split[0])) {
      return {self.children->set(split[0], std::move(value))};
    } else {
      return make<0>(split, std::move(value));
    }
  }
  
  
  template<std::size_t D>
  static node_type<D> set(const index_array& split, const node_type<D>& self, T&& value) {
    assert(self.children);
    if(self.children->has(split[D])) {
      return {self.children->set(split[D],
                                 set(split,
                                     self.children->get(split[D]),
                                     std::move(value)))};
    } else {
      return make<D>(split, std::move(value));
    }
  }


  
public:
  hamt() = default;
  
  const T& get(std::size_t index) const {
    const index_array split = hamt::split(index, level_indices{});
    return get(split, root);
  }


  hamt set(std::size_t index, T&& value) const {
    const index_array split = hamt::split(index, level_indices{});
    return {set(split, root, std::move(value))};
  }
  

  
};


int main(int, char**) {
  using value_type = double;
  std::shared_ptr<array<value_type>> array = std::make_shared<storage<value_type, 10, 1>>(1, value_type{5}); 
  
  array = array->set(0, 10);
  array = array->set(4, 14);  
  std::clog << array->get(4) << std::endl;

  //
  hamt<double, 5, 5> test;
  test.set(0, 1);
  
  return 0;
}
