// -*- compile-command: "c++ -std=c++14 hamt2.cpp -o hamt -lstdc++ -g" -*-

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


  template<class T, class U, std::size_t... Is>
  static std::array<T, sizeof...(Is)> set(std::array<T, sizeof...(Is)>&& values,
                                          std::size_t index, U&& value,
                                          std::index_sequence<Is...>) {
    return {std::move((Is == index) ? value : std::get<Is>(values))...};
  }


  template<class T, class U, std::size_t... Is>
  static std::array<T, sizeof...(Is)> set(const std::array<T, sizeof...(Is)>& values,
                                          std::size_t index, const U& value,
                                          std::index_sequence<Is...>) {
    return {((Is == index) ? value : std::get<Is>(values))...};
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
  using node::chunk::chunk;
  // inner node
};


template<class T, std::size_t B, std::size_t L>
struct node<T, B, L, 0>: chunk<T, (1ul << L)> {
  using node::chunk::chunk;
  // leaf node
};



template<class T, std::size_t B, std::size_t L>
class hamt {
  static constexpr std::size_t bits = sizeof(std::size_t) * 8;
  static_assert(L <= bits, "size error");
  
  static constexpr std::size_t inner_levels = (bits - L) / B;  
  static constexpr std::size_t total_levels = 1 + inner_levels;
  static_assert(L + inner_levels * B <= bits, "size error");
  
  template<std::size_t D> using node_type = node<T, B, L, D>;

  // root node is toplevel
  using root_type = node_type<total_levels - 1>;
  ref<root_type> root;

  public:
  bool empty() const { return !bool(root); }

  const T& get(std::size_t index) const {
    const index_array split = hamt::split(masks, offsets, index, level_indices{});
    return get(split, root.get());
  }

  hamt set(std::size_t index, T&& value) const {
    const index_array split = hamt::split(masks, offsets, index, level_indices{});
    return hamt::set(split, root.get(), std::move(value));
  }

  hamt(): hamt(nullptr) { }
  
private:
  hamt(ref<root_type> root): root(std::move(root)) {}

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

  template<std::size_t... Ds>
  static index_array split(const index_array& masks,
                           const index_array& offsets,
                           std::size_t index,
                           std::index_sequence<Ds...>) {
    return {((index & masks[Ds]) >> offsets[Ds])...};
  }

  static constexpr index_array masks = make_masks(level_indices{});
  static constexpr index_array offsets = make_offsets(level_indices{});

  template<std::size_t D, class ... Args>
  static auto make_node(Args&& ... args) {
    return std::make_shared<node_type<D>>(std::forward<Args>(args)...);
  }

  // child access
  static const T& child(const node_type<0>* self, std::size_t index) {
    return self->values[index];
  }

  
  template<std::size_t D>
  static const node_type<D - 1>* child(const node_type<D>* self, std::size_t index) {
    return static_cast<const node_type<D - 1>*>(self->values[index].get());
  }


  // get
  static const T& get(const index_array& split, const node_type<0>* self) {
    return child(self, split[0]);
  }
  
  template<std::size_t D>
  static const T& get(const index_array& split, const node_type<D>* self) {
    return get(split, child(self, split[D]));
  }


  // set
  static ref<node_type<0>> set(const index_array& split, const node_type<0>* self, T&& value) {
    constexpr auto indices = std::make_index_sequence<(1 << L)>{};
    if(self) {
      return make_node<0>(detail::set(self->values, split[0], std::move(value), indices)); 
    } else {
      return make_node<0>(split[0], std::move(value), indices);
    }
  }
    
  template<std::size_t D>
  static ref<node_type<D>> set(const index_array& split, const node_type<D>* self, T&& value) {
    constexpr auto indices = std::make_index_sequence<(1 << B)>{};    
    if(self) {
      return make_node<D>(detail::set(self->values, split[D],
                                      set(split, child(self, split[D]), std::move(value)), indices)); 
    } else {
      return make_node<D>(split[D], set(split, nullptr, std::move(value)), indices);
    }
  }
};

template<class T, std::size_t B, std::size_t L>
constexpr typename hamt<T, B, L>::index_array hamt<T, B, L>::masks;

template<class T, std::size_t B, std::size_t L>
constexpr typename hamt<T, B, L>::index_array hamt<T, B, L>::offsets;


int main(int, char**) {
  using hamt_type = hamt<double, 5, 5>;
  hamt_type x;

  x = x.set(0, 0);
  x = x.set(2, 0);
  return 0;
}
