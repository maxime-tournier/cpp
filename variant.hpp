#ifndef VARIANT_HPP
#define VARIANT_HPP

#include <cassert>
#include <memory>

#include "overload.hpp"

namespace detail {

template<std::size_t I, class T>
struct type_index {
    friend constexpr std::integral_constant<std::size_t, I> get_index(type_index, const T*) { return {}; }
};

template<class Is, class... Ts>
struct type_indices;

template<std::size_t... Is, class... Ts>
struct type_indices<std::index_sequence<Is...>, Ts...>: type_index<Is, Ts>... { };

} // namespace detail


template<class... Args>
class variant {
    struct base {
        const std::size_t index;
    };

    template<class T>
    struct derived: base {
        const T value;

        derived(std::size_t index, const T& value): base{index}, value{value} {}
    };

    std::shared_ptr<const base> storage;
    using type_index = detail::type_indices<std::index_sequence_for<Args...>, Args...>;

public:
    template<class T>
    variant(const T& value):
      storage(std::make_shared<derived<T>>(get_index(type_index{}, &value).value, value)) {}

    variant(const variant&) = default;
    variant(variant&&) = default;

    template<class Visitor>
    friend auto visit(const variant& self, Visitor visitor)  {
        using result_type =
            typename std::common_type<typename std::result_of<Visitor(const Args&)>::type...>::type;
        using thunk_type = result_type (*)(const base*, const Visitor& visitor);

        const thunk_type table[] = {+[](const base* ptr, const Visitor& visitor) -> result_type {
          return visitor(static_cast<const derived<Args>*>(ptr)->value);
        }...};

        return table[self.storage->index](self.storage.get(), visitor);
    }

    template<class... Cases>
    friend auto match(const variant& self, Cases... cases) {
      return visit(self, overload<Cases...>{cases...});
    }

    template<class T>
    const T* cast() const {
      if(storage->index == get_index(type_index{}, (const T*)nullptr)) {
        return &get<T>();
      }

      return nullptr;
    }

    template<class T>
    const T& get() const {
      return static_cast<const derived<T>*>(storage.get())->value;
    }

    std::size_t type() const { return storage->index; }
    std::size_t id() const { return std::size_t(storage.get()); }
};


#endif
