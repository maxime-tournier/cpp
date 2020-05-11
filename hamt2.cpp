// -*- compile-command: "c++ -std=c++14 hamt2.cpp -o hamt" -*-

#include <array>
#include <memory>

template <class T>
using ref = std::shared_ptr<T>;

struct base {
    virtual ~base() { }
};

template <class T, std::size_t... Is>
static std::array<T, sizeof...(Is) + 1> push(const std::array<T, sizeof...(Is)>& firsts, T&& last) {
    return {std::get<Is>(firsts)..., std::move(last)};
}

template <class T, std::size_t... Is>
static std::array<T, sizeof...(Is) + 1> push(std::array<T, sizeof...(Is)>&& firsts, T&& last) {
    return {std::move(std::get<Is>(firsts))..., std::move(last)};
}


template <class T, std::size_t... Is>
static std::array<T, sizeof...(Is)> set(std::array<T, sizeof...(Is)>&& firsts, std::size_t index, T&& value) {
    return { std::move(Is == index ? value : std::get<Is>(firsts))...};
}


template <class T, std::size_t... Is>
static std::array<T, sizeof...(Is)> set(const std::array<T, sizeof...(Is)>& firsts, std::size_t index, const T& value) {
    return { (Is == index ? value : std::get<Is>(firsts))...};
}


template<class T, std::size_t N>
struct chunk: base {
    using values_type = std::array<T, N>;
    values_type values;
    chunk(values_type values): values(std::move(values)) { }
    
};


template <class T, std::size_t B, std::size_t L, std::size_t D>
struct node: chunk<ref<base>, (1ul << B)> {
    using node::chunk::node;
    // inner node
};


template <class T, std::size_t B, std::size_t L>
struct node<T, B, L, 0>: chunk<T, (1ul << L)> {
    using node::chunk::node;    
    // leaf node
};


namespace detail {
  template<std::size_t B, std::size_t L, std::size_t D>
  static constexpr std::size_t mask() {
    return ((1ul << (L + 1)) - 1) << (D * B);
  }

  template<std::size_t B, std::size_t L, std::size_t D>
  static constexpr std::size_t offset() {
    return D ? L + (D - 1) * B : 0;
  }


  template<std::size_t B, std::size_t L, std::size_t ... Ds>
  static constexpr std::array<std::size_t, sizeof...(Ds)> make_masks(std::index_sequence<Ds...>) {
    return {mask<B, L, Ds>()...};
  }

  template<std::size_t B, std::size_t L, std::size_t ... Ds>
  static constexpr std::array<std::size_t, sizeof...(Ds)> make_offsets(std::index_sequence<Ds...>) {
    return {offset<B, L, Ds>()...};
  }

  template<std::size_t B, std::size_t L, std::size_t ... Ds>
  static std::array<std::size_t, sizeof...(Ds)> split(const std::array<std::size_t, sizeof...(Ds)>& masks,
                                                      const std::array<std::size_t, sizeof...(Ds)>& offsets,
                                                      std::size_t index, std::index_sequence<Ds...>) {
    return {((index & masks[Ds]) >> offsets[Ds])...};
  }
  
}



template <class T, std::size_t B, std::size_t L>
class hamt {
    static constexpr std::size_t bits = sizeof(std::size_t) * 8;
    static_assert(L <= bits, "size error");
    static constexpr std::size_t depth = (bits - L) / B;

    using indices = std::make_index_sequence<depth>;
    static constexpr auto masks = make_masks<B, L>(indices{});
    static constexpr auto offsets = make_offsets<B, L>(indices{});

    using root_type = node<T, B, L, depth>;
    ref<root_type> root;
    
public:
    bool empty() const { return !bool(root); }

    const T& get(std::size_t index) const {
        auto split = detail::split(masks, offsets, index, indices{});
        return get(split, root);
    }
};



int main(int, char** ) {
    
    return 0;
}
