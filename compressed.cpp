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

  // root node is toplevel
  using node_type = ref<base>;
  node_type root;
  hamt(node_type root): root(std::move(root)) { }
  
  using index_array = typename hamt::traits::index_array;
  using level_indices = typename hamt::traits::level_indices;  

  template<std::size_t D=hamt::inner_levels>
  struct go {
    using next = go<D - 1>;
    
    static const array<node_type>* cast(const base* self) {
      return static_cast<const array<node_type>*>(self);
    }

    static const T& get(const index_array& split, const node_type& self) {
      return next::get(split, cast(self.get())->get(split[D]));
    }

    static node_type make(const index_array& split, T&& value) {
      using children_type = storage<node_type, (1ul << B), 1>;
      
      return std::make_shared<children_type>(1ul << split[D],
                                             next::make(split, std::move(value)));
    }

    static node_type set(const index_array& split, const node_type& self, T&& value) {
      if(self->has(split[D])) {
        return cast(self.get())->set(split[D],
                                     next::set(split,
                                               cast(self.get())->get(split[D]),
                                               std::move(value)));
      } else {
        return make(split, std::move(value));
      }
    }
  };

  
  template<>
  struct go<0> {

    static const array<T>* cast(const base* self) {
      return static_cast<const array<T>*>(self);
    }
    
    static const T& get(const index_array& split, const node_type& self) {
      return cast(self.get())->get(split[0]);
    }

    static node_type make(const index_array& split, T&& value) {
      using children_type = storage<T, (1ul << L), 1>;
      return std::make_shared<children_type>(1ul << split[0], std::move(value));
    }

    static node_type set(const index_array& split,
                         const node_type& self, T&& value) {
      if(self->has(split[0])) {
        return cast(self.get())->set(split[0], std::move(value));
      } else {
        return make(split, std::move(value));
      }
    }
  };
  

  
public:
  hamt() = default;
  
  const T& get(std::size_t index) const {
    const index_array split = hamt::split(index, level_indices{});
    return go<>::get(split, root);
  }


  hamt set(std::size_t index, T&& value) const {
    const index_array split = hamt::split(index, level_indices{});
    return {go<>::set(split, root, std::move(value))};
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
