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


template <class T, std::size_t B, std::size_t L>
class hamt {
    static constexpr std::size_t bits = sizeof(std::size_t) * 8;
    static_assert(L <= bits, "size error");
    static constexpr std::size_t depth = (bits - L) / B;

    using root_type = node<T, B, L, depth>;
    ref<root_type> root;
public:
    bool empty() const { return !bool(root); }
};



int main(int, char** ) {
    
    return 0;
}
