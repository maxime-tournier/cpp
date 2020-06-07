// -*- compile-command: "c++ -std=c++14 compressed.cpp -o compressed -lstdc++ -g" -*-

#include "compressed.hpp"
#include <iostream>

template<class T>
using ref = std::shared_ptr<T>;

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
  static index_array split(std::size_t index, std::index_sequence<Ds...>) {
    return {((index & masks[Ds]) >> offsets[Ds])...};
  }
};


template<std::size_t B, std::size_t L>
constexpr typename traits<B, L>::index_array traits<B, L>::masks;

template<std::size_t B, std::size_t L>
constexpr typename traits<B, L>::index_array traits<B, L>::offsets;


template<class T, std::size_t B, std::size_t L>
class hamt: public traits<B, L> {
  using node_type = ref<sparse::base>;
  node_type root;
  hamt(node_type root): root(std::move(root)) {}

  using index_array = typename hamt::traits::index_array;
  using level_indices = typename hamt::traits::level_indices;

  // cast
  static sparse::array<T>* as_leaf(const node_type& self) {
    return static_cast<sparse::array<T>*>(self.get());
  }

  static sparse::array<node_type>* as_inner(const node_type& self) {
    return static_cast<sparse::array<node_type>*>(self.get());
  }

  // get
  static const T& get(const index_array& split, const node_type& self,
                      std::size_t level = hamt::inner_levels) {
    if(!level) {
      return as_leaf(self)->get(split[level]);
    } else {
      return get(split, as_inner(self)->get(split[level]), level - 1);
    }
  }


  // find
  static const T* find(const index_array& split, const node_type& self,
                       std::size_t level = hamt::inner_levels) {
    if(!self->has(split[level])) {
      return nullptr;
    }

    if(!level) {
      return &as_leaf(self)->get(split[level]);
    } else {
      return find(split, as_inner(self)->get(split[level]), level - 1);
    }
  }

  // make
  static node_type make(const index_array& split, T&& value,
                        std::size_t level = hamt::inner_levels) {
    if(!level) {
      using children_type = sparse::storage<T, (1ul << L), 1>;
      return std::make_shared<children_type>(1ul << split[level],
                                             std::move(value));
    } else {
      using children_type = sparse::storage<node_type, (1ul << B), 1>;
      return std::make_shared<children_type>(
          1ul << split[level], make(split, std::move(value), level - 1));
    }
  }

  // set
  static node_type set(const index_array& split, const node_type& self,
                       T&& value, std::size_t level = hamt::inner_levels) {
    if(!level) {
      return as_leaf(self)->set(split[level], std::move(value));
    } else if(self->has(split[level])) {
      return as_inner(self)->set(split[level],
                                 set(split, as_inner(self)->get(split[level]),
                                     std::move(value), level - 1));
    } else {
      return as_inner(self)->set(split[level],
                                 make(split, std::move(value), level - 1));
    }
  }

  // emplace
  static node_type emplace(const index_array& split, node_type self,
                           T&& value, std::size_t level = hamt::inner_levels) {
    if(!self.unique() || !self->has(split[level])) {
      return set(split, self, std::move(value), level);
    }

    if(!level) {
      as_leaf(self)->get(split[level]) = std::move(value);
    } else {
      node_type& node = as_inner(self)->get(split[level]);
      node = emplace(split, std::move(node), std::move(value), level - 1);
    }
    
    return self;
  }

  
  public:
  hamt() = default;

  const T& get(std::size_t index) const {
    const index_array split = hamt::split(index, level_indices{});
    return get(split, root);
  }


  hamt set(std::size_t index, T&& value) const& {
    const index_array split = hamt::split(index, level_indices{});
    if(!root) {
      return make(split, std::move(value));
    }

    return set(split, root, std::move(value));
  }


  hamt set(std::size_t index, T&& value) && {
    const index_array split = hamt::split(index, level_indices{});
    if(!root) {
      return make(split, std::move(value));
    }

    return emplace(split, std::move(root), std::move(value));
  }

  
  const T* find(std::size_t index) const {
    if(!root) {
      return nullptr;
    }
    const index_array split = hamt::split(index, level_indices{});
    return find(split, root);
  }
  
};


#include <chrono>
#include <map>
#include <unordered_map>
#include <vector>

template<class Action, class Resolution = std::chrono::milliseconds,
		 class Clock = std::chrono::high_resolution_clock>
static double time(Action action) {
  typename Clock::time_point start = Clock::now();
  action();
  typename Clock::time_point stop = Clock::now();

  std::chrono::duration<double> res = stop - start;
  return res.count();
}




template<class T, std::size_t B, std::size_t L>
static void test_ordered(std::size_t size) {
  std::cout << "vector: " << time([=] {
    std::vector<T> values;
    for(std::size_t i = 0; i < size; ++i) {
      values.emplace_back(i);
    }
    
    return values.size();
  }) << std::endl;


  std::cout << "map: " << time([=] {
    std::map<std::size_t, T> values;
    for(std::size_t i = 0; i < size; ++i) {
      values.emplace(i, i);
    }
    
    return values.size();
  }) << std::endl;

  std::cout << "unordered_map: " << time([=] {
    std::unordered_map<std::size_t, T> values;
    for(std::size_t i = 0; i < size; ++i) {
      values.emplace(i, i);
    }
    
    return values.size();
  }) << std::endl;
  
  std::cout << "hamt: " << time([=] {
    hamt<T, B, L> values;
    for(std::size_t i = 0; i < size; ++i) {
      values = values.set(i, i);
    }
  }) << std::endl;


  std::cout << "hamt emplace: " << time([=] {
    hamt<T, B, L> values;
    for(std::size_t i = 0; i < size; ++i) {
      values = std::move(values).set(i, i);
    }
  }) << std::endl;
  
}






int main(int, char**) {
  //
  hamt<double, 5, 5> test;
  test = test.set(0, 1);
  std::clog << test.find(0) << std::endl;
  std::clog << test.find(1) << std::endl;

  test_ordered<double, 5, 4>(400000);
  
  return 0;
}
