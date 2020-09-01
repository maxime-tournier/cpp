#ifndef HAMT_HPP
#define HAMT_HPP

#include "compressed.hpp"

namespace hamt {

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
      return ((1ul << B) - 1) << (L + (D - 1) * B);
    } else {
      return (1ul << L) - 1;
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


template<class T, std::size_t B=5, std::size_t L=4>
class array: public traits<B, L> {
  using node_type = ref<sparse::base>;
  node_type root;
  array(node_type root): root(std::move(root)) {}

  using index_array = typename array::traits::index_array;
  using level_indices = typename array::traits::level_indices;

  // cast
  static sparse::array<T>* as_leaf(const node_type& self) {
    return static_cast<sparse::array<T>*>(self.get());
  }

  static sparse::array<node_type>* as_inner(const node_type& self) {
    return static_cast<sparse::array<node_type>*>(self.get());
  }

  // get
  static const T& get(const index_array& split, const node_type& self,
                      std::size_t level = array::inner_levels) {
    if(!level) {
      return as_leaf(self)->get(split[level]);
    } else {
      return get(split, as_inner(self)->get(split[level]), level - 1);
    }
  }


  // find
  static const T* find(const index_array& split, const node_type& self,
                       std::size_t level = array::inner_levels) {
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
                        std::size_t level = array::inner_levels) {
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
                       T&& value, std::size_t level = array::inner_levels) {
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
                           T&& value, std::size_t level = array::inner_levels) {
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


  // iter
  template<class Cont>
  static void iter(const node_type& self,
                   const Cont& cont,
                   std::size_t start=0,
                   std::size_t level = array::inner_levels) {
    if(!level) {
      as_leaf(self)->iter(start << L, cont);  
    } else {
      as_inner(self)->iter(start << B, [&](std::size_t i,
                                           const node_type& child) {
        iter(child, cont, i, level - 1);
      });
    }
  }
  
public:
  array() = default;

  const T& get(std::size_t index) const {
    const index_array split = array::split(index, level_indices{});
    return get(split, root);
  }


  array set(std::size_t index, T&& value) const& {
    const index_array split = array::split(index, level_indices{});
    if(!root) {
      return make(split, std::move(value));
    }

    return set(split, root, std::move(value));
  }


  array set(std::size_t index, T&& value) && {
    const index_array split = array::split(index, level_indices{});
    if(!root) {
      return make(split, std::move(value));
    }

    return emplace(split, std::move(root), std::move(value));
  }

  
  const T* find(std::size_t index) const {
    if(!root) {
      return nullptr;
    }
    const index_array split = array::split(index, level_indices{});
    return find(split, root);
  }

  explicit operator bool() const {
    return bool(root);
  }


  template<class Cont>
  void iter(const Cont& cont) const {
    if(!root) {
      return;
    }

    iter(root, cont);
  }
  
};


template<class Key, class Value,
         std::size_t B=5, std::size_t L=4>
class map {
public:
  using key_type = Key;
  using value_type = Value;

  static_assert(std::is_trivially_destructible<key_type>::value &&
                std::is_trivially_copy_constructible<key_type>::value &&
                std::is_trivially_move_constructible<key_type>::value,
                "key type must be POD");
  
  static_assert(sizeof(key_type) <= sizeof(std::size_t),
                "key type is too large");

private:
  using array_type = array<value_type, B, L>;
  array_type array;

  union cast {
    key_type key;
    std::size_t index;
    cast(key_type key): key(key) { }
    // TODO make this work if key_type == std::size_t
    cast(std::size_t index): index(index) { }      
  };
  
  static std::size_t index(key_type key) {
    return cast(key).index;
  }

  map(array_type array): array(std::move(array)) { }

public:
  map() = default;
  
  const value_type* find(key_type key) const {
    return array.find(index(key));
  }

  map set(key_type key, value_type&& value) && {
    return std::move(array).set(index(key), std::move(value));
  }

  map set(key_type key, value_type&& value) const& {
    return array.set(index(key), std::move(value));
  }

  const value_type& get(key_type key) const {
    return array.get(index(key));
  }

  explicit operator bool() const {
    return bool(array);
  }

  template<class Cont>
  void iter(const Cont& cont) const {
    array.iter([&](std::size_t index, const value_type& value) {
      cont(cast(index).key, value);
    });
  }
  
};



}


#endif
