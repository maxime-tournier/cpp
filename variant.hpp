#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <cassert>
#include <memory>

#include "overload.hpp"


struct variant_base {
  virtual ~variant_base() {}
  variant_base(std::size_t index): index(index) {}
  const std::size_t index;
};

template<class T>
struct variant_derived: variant_base {
  const T value;

  template<class Arg>
  variant_derived(std::size_t index, const Arg& arg):
      variant_base(index), value(arg) {}
};


template<std::size_t I, class T>
struct variant_item {
  template<class Derived>
  friend constexpr std::integral_constant<std::size_t, I>
  get_index(const Derived*, const T&) {}

  template<class Derived>
  friend constexpr T get_type(const Derived*, std::index_sequence<I>);

  template<class Derived>
  friend std::shared_ptr<variant_base> construct(const Derived*,
                                                 const T& value) {
    return std::make_shared<variant_derived<T>>(I, value);
  }
};

template<class Indices, class... Ts>
struct variant_items;


template<std::size_t... Is, class... Ts>
struct variant_items<std::index_sequence<Is...>, Ts...>
    : variant_item<Is, Ts>... {
  template<class T>
  using index =
      decltype(get_index((variant_items*)nullptr, std::declval<const T&>()));

  template<std::size_t J>
  using type =
      decltype(get_type((variant_items*)nullptr, std::index_sequence<J>{}));
};


// el-cheapo immutablo varianto
template<class... Ts>
class variant: variant_items<std::index_sequence_for<Ts...>, Ts...> {
  using indices_type = std::index_sequence_for<Ts...>;

  using data_type = std::shared_ptr<variant_base>;
  data_type data;

  public:
  variant(const variant&) = default;
  variant(variant&&) = default;

  variant& operator=(const variant&) = default;
  variant& operator=(variant&&) = default;

  template<class Arg>
  variant(const Arg& value): data(construct(this, value)) {}

  std::size_t type() const { return data->index; }

  template<class T, std::size_t index = variant::template index<T>::value>
  const T& get() const {
    assert(data->index == index && "type error");
    return static_cast<variant_derived<T>*>(data.get())->value;
  }

  template<class T, std::size_t index = variant::template index<T>::value>
  const T* as() const {
    if(data->index != index)
      return nullptr;
    return &get<T>();
  }

  template<class Visitor, class... Args>
  friend auto visit(const variant& self, const Visitor& visitor,
                    Args&&... args) {
    using result_type = std::common_type_t<decltype(
        visitor(self.get<Ts>(), std::forward<Args>(args)...))...>;

    using thunk_type =
        result_type (*)(const variant&, const Visitor& visitor, Args&&...);
    // TODO should be constexpr
    static const thunk_type thunks[] = {[](const variant& self,
                                           const Visitor& visitor,
                                           Args&&... args) -> result_type {
      return visitor(self.get<Ts>(), std::forward<Args>(args)...);
    }...};

    return thunks[self.data->index](self, visitor, std::forward<Args>(args)...);
  };


  template<class... Cases>
  friend auto match(const variant& self, const Cases&... cases) {
    return visit(self, overload<Cases...>(cases...));
  };
};


#endif
