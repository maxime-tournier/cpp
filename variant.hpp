#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <cassert>
#include <memory>

#include "overload.hpp"


template<std::size_t N, class T, class ... Ts>
struct type_index;

template<std::size_t N, class T, class ... Ts>
struct type_index<N, T, T, Ts...> {
  static_assert(1 + sizeof...(Ts) <= N, "size error");
  static constexpr std::size_t value = N - (sizeof...(Ts) + 1);
};

template<std::size_t N, class T, class X, class ... Xs>
struct type_index<N, T, X, Xs...>: type_index<N, T, Xs...> { };


// el-cheapo immutablo varianto
template<class... Ts>
class variant {
  struct base {
    virtual ~base() {}
    base(std::size_t index): index(index) {}
    const std::size_t index;
  };

  template<class T>
  struct derived: base {
    const T value;
    
    derived(std::size_t index, const T& value):
      base(index), value(value) {}
  };

  template<class T>
  struct tag {
    friend std::shared_ptr<base> construct(const tag& self,
                                           const T* value) {
      return std::make_shared<derived<T>>(index(self, value), *value);
    }

    friend constexpr std::size_t index(const tag&,
                                       const T* value) {
      return type_index<sizeof...(Ts), T, Ts...>::value;
    }
  };

  struct tags: tag<Ts>... {
    template<class T>
    static constexpr std::size_t index_of() {
      return index(tags{}, (const T*)nullptr);
    }
  };

  using data_type = std::shared_ptr<base>;
  data_type data;

public:
  variant(const variant&) = default;
  variant(variant&&) = default;

  variant& operator=(const variant&) = default;
  variant& operator=(variant&&) = default;

  template<class T>
  variant(const T& value): data(construct(tags{}, &value)) {}

  std::size_t type() const { return data->index; }

  template<class T, std::size_t I = tags::template index_of<T>()>
  const T& get() const {
    assert(data->index == I && "type error");
    return static_cast<derived<T>*>(data.get())->value;
  }

  template<class T, std::size_t I = tags::template index_of<T>()>
  const T* cast() const {
    if(data->index != I) {
      return nullptr;
    }
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


  bool operator==(const variant& other) const {
    if(type() != other.type()) {
      return false;
    }

    return match(*this, [&](const auto& self) {
      using type = typename std::decay<decltype(self)>::type;
      return self == other.get<type>();
    });
  }
  
  
};


#endif
