// -*- compile-command: "c++ -std=c++14 hamt2.cpp -o hamt" -*-

#include <array>
#include <memory>

template<class T>
using ref = std::shared_ptr<T>;

struct base {
  virtual ~base() {}
};

namespace detail {
  template<class T, std::size_t... Is>
  static std::array<T, sizeof...(Is) + 1> push(const std::array<T, sizeof...(Is)>& firsts, T&& last) {
    return {std::get<Is>(firsts)..., std::move(last)};
  }

  template<class T, std::size_t... Is>
  static std::array<T, sizeof...(Is) + 1> push(std::array<T, sizeof...(Is)>&& firsts, T&& last) {
    return {std::move(std::get<Is>(firsts))..., std::move(last)};
  }


  template<class T, std::size_t... Is>
  static std::array<T, sizeof...(Is)> set(std::array<T, sizeof...(Is)>&& firsts, std::size_t index,
                                          T&& value) {
    return {std::move(Is == index ? value : std::get<Is>(firsts))...};
  }


  template<class T, std::size_t... Is>
  static std::array<T, sizeof...(Is)> set(const std::array<T, sizeof...(Is)>& firsts, std::size_t index,
                                          const T& value) {
    return {(Is == index ? value : std::get<Is>(firsts))...};
  }
}

template<class T, std::size_t N>
struct chunk: base {
  using values_type = std::array<T, N>;
  values_type values;
  
  chunk(values_type values): values(std::move(values)) {}

  template<std::size_t ... Is>
  chunk(std::size_t index, T&& value, std::index_sequence<Is...>):
    values{std::move(index == Is ? value : T{})...} {}
};


template<class T, std::size_t B, std::size_t L, std::size_t D>
struct node: chunk<ref<base>, (1ul << B)> {
  using node::chunk::node;
  // inner node
};


template<class T, std::size_t B, std::size_t L>
struct node<T, B, L, 0>: chunk<T, (1ul << L)> {
  using node::chunk::node;
  // leaf node
};



template<class T, std::size_t B, std::size_t L>
class hamt {
  static constexpr std::size_t bits = sizeof(std::size_t) * 8;
  static_assert(L <= bits, "size error");
  static constexpr std::size_t depth = (bits - L) / B;

  using root_type = node<T, B, L, depth>;
  ref<root_type> root;

  public:
  bool empty() const { return !bool(root); }

  const T& get(std::size_t index) const {
    const index_array split = hamt::split(masks, offsets, index, indices{});
    return get(split, root.get());
  }

  hamt set(std::size_t index, const T& value) const {
    const index_array split = hamt::split(masks, offsets, index, indices{});
    return set(split, root.get());
  }

  hamt(ref<root_type> root): root(std::move(root)) {}
  
private:
  using indices = std::make_index_sequence<depth>;

  template<std::size_t D>
  static constexpr std::size_t mask() {
    return ((1ul << (L + 1)) - 1) << (D * B);
  }

  template<std::size_t D>
  static constexpr std::size_t offset() {
    return D ? L + (D - 1) * B : 0;
  }

  using index_array = std::array<std::size_t, depth>;

  template<std::size_t... Ds>
  static constexpr index_array make_masks(std::index_sequence<Ds...>) {
    return {mask<Ds>()...};
  }

  template<std::size_t... Ds>
  static constexpr index_array make_offsets(std::index_sequence<Ds...>) {
    return {offset<Ds>()...};
  }

  template<std::size_t... Ds>
  static index_array split(const index_array& masks,
                           const index_array& offsets,
                           std::size_t index,
                           std::index_sequence<Ds...>) {
    return {((index & masks[Ds]) >> offsets[Ds])...};
  }


  static constexpr index_array masks = make_masks(indices{});
  static constexpr index_array offsets = make_offsets(indices{});

  static const T& get(const index_array& split, const node<T, B, L, 0>* self) {
    return self->values[split[0]];
  }
  
  template<std::size_t D>
  static const T& get(const index_array& split, const node<T, B, L, D>* self) {
    const base* child = self->values[split[D]].get();
    return get(split, static_cast<const node<T, B, L, D - 1>*>(child));
  }


  static ref<node<T, B, L, 0>> make(const index_array& split, T&& value,
                                                std::integral_constant<std::size_t, 0>) {
    return std::make_shared<node<T, B, L, 0>>(split[0], std::move(value), std::make_index_sequence<L>{});
  }

  template<std::size_t D>
  static ref<node<T, B, L, D>> make(const index_array& split, T&& value,
                                    std::integral_constant<std::size_t, D>) {
    return std::make_shared<node<T, B, L, D>>(split[D], make(split, std::move(value),
                                                             std::integral_constant<std::size_t, D - 1>{}),
                                              std::make_index_sequence<B>{});
  }

  static ref<node<T, B, L, 0>> set(const index_array& split, const node<T, B, L, 0>* self, T&& value) {
    return std::make_shared<node<T, B, L, 0>>(detail::set(self->values, split[0], std::move(value),
                                                          std::make_index_sequence<L>{}));
  }
    
  template<std::size_t D>
  static ref<node<T, B, L, D>> set(const index_array& split, const node<T, B, L, D>* self, T&& value) {
    ref<node<T, B, L, D - 1>> new_child;
    if(auto child = self->values[split[D]].get()) {
      new_child = set(split, static_cast<const node<T, B, L, D - 1>*>(child), std::move(value));
    } else {
      new_child = make<D - 1>(split, std::move(value));
    }

    return std::make_shared<node<T, B, L, D>>(detail::set(self->values, split[D], std::move(new_child),
                                                          std::make_index_sequence<B>{}));
  }
  
};


int main(int, char**) {
  using hamt_type = hamt<double, 5, 5>;
  hamt_type x;
  return 0;
}
