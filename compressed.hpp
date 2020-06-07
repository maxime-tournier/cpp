#ifndef COMPRESSED_HPP
#define COMPRESSED_HPP

#include <array>
#include <memory>
#include <cassert>

namespace sparse {

static std::size_t index(std::size_t mask, std::size_t index) {
  const std::size_t res = __builtin_popcount(mask & ((1ul << index) - 1));
  return res;
}


class base {
protected:
  const std::size_t mask;
  base(std::size_t mask): mask(mask) {}

public:
  virtual ~base() {}

  std::size_t size() const { return __builtin_popcount(mask); }

  bool has(std::size_t index) const { return mask & (1ul << index); }
};

template<class T>
class array: public base {
  using base::base;
  const T data[0] = {};

public:
  const T& get(std::size_t index) const {
    assert(((1ul << index) & this->mask) && "index error");
    return data[sparse::index(this->mask, index)];
  }

  virtual std::shared_ptr<array> set(std::size_t index, T&& value) const = 0;
};


template<class T, std::size_t M, std::size_t N>
struct storage;

template<class T, std::size_t M, class... Args>
static std::enable_if_t<(sizeof...(Args) <= M),
                        std::shared_ptr<storage<T, M, sizeof...(Args)>>>
make_storage(std::size_t mask, Args&&... args) {
  return std::make_shared<storage<T, M, sizeof...(Args)>>(
      mask, std::forward<Args>(args)...);
}

template<class T, std::size_t M, class... Args>
static std::enable_if_t<(sizeof...(Args) > M), std::shared_ptr<array<T>>>
make_storage(std::size_t mask, Args&&... args) {
  throw std::logic_error("size error");
}


template<class T, std::size_t M, std::size_t N>
struct storage: array<T> {
  static_assert(N > 0, "empty array");
  static_assert(N <= M, "size error");

  using values_type = std::array<T, N>;
  values_type values;

  // general
  template<class... Args>
  storage(std::size_t mask, Args&&... args):
      array<T>(mask), values{std::forward<Args>(args)...} {
    static_assert(sizeof...(Args) == N, "size error");
  }


  template<std::size_t... Is>
  std::shared_ptr<array<T>> set(std::size_t index, T&& value,
                                std::index_sequence<Is...>) const {
    const std::size_t compressed = sparse::index(this->mask, index);

    if(this->mask & (1ul << index)) {
      return make_storage<T, M>(
          this->mask,
          ((Is == compressed) ? std::move(value) : T{values[Is]})...);
    } else {
      // insert value into values: {values[Is]..., value, values[Is - 1]...}
      return make_storage<T, M>(
          this->mask | (1ul << index),
          ((Is == compressed) ? std::move(value)
                              : T{values[Is > compressed ? Is - 1 : Is]})...,
          ((compressed == N) ? std::move(value) : T{values[N - 1]}));
    }
  }

  std::shared_ptr<array<T>> set(std::size_t index, T&& value) const override {
    return set(index, std::move(value), std::make_index_sequence<N>{});
  }
};

} // namespace sparse

#endif
