#ifndef AMT_HPP
#define AMT_HPP

#include "sparse_array.hpp"

struct base {
  const std::shared_ptr<void> storage;

  template<class T>
  base(std::shared_ptr<T> storage): storage(std::move(storage)) { }

  base() { }
};


template<class T, std::size_t level, std::size_t B, std::size_t L>
struct node: base {
  static_assert(level > 0, "level error");
  using child_node = node<T, level - 1, B, L>;

  using children_type = sparse::base<child_node>;
  using single_type = sparse::derived<child_node, 1>;

  static constexpr std::size_t children_size = 1ul << B;
  static constexpr std::size_t capacity = 1ul << (L + B * level);
  
  static constexpr std::size_t shift = L + B * (level - 1);
  static constexpr std::size_t mask = (1ul << (shift + 1)) - 1;
  
  const children_type* children() const { return reinterpret_cast<const children_type*>(storage.get()); };

  static children_type make_children(std::size_t index, const T& value) {
    return split(index, [&](std::size_t sub, std::size_t rest) {
      return std::make_shared<single_type>(1ul << sub, child_node(rest, value));
    });
  }
  
  node(std::size_t index, const T& value):
    base(make_children(index, value)) { }
  
  
  template<class Func>
  auto split(std::size_t index, const Func& func) {
    const std::size_t sub = index >> shift;
    const std::size_t rest = index & mask;
    return func(sub, rest);
  }
  
  const T& get(std::size_t index) const {
    return split([&](std::size_t sub, std::size_t rest) {
      assert(children()->contains(sub));
      return children()->get(sub).get(rest);
    });
  }

  node set(std::size_t index, const T& value) const {
    return split([&](std::size_t sub, std::size_t rest) {
      if(children()->contains(sub)) {
        return children()->set(sub, children()->get(sub).set(rest, value));
      } else {
        return children()->set(sub, child_node(rest, value));
      }
    });
  }

  node<T, level + 1, B, L> raise() const { return {0, *this}; }  
};


template<class T, std::size_t B, std::size_t L>
struct node<T, 0, B, L>: base {
  using children_type = sparse::base<T>;
  using single_type = sparse::derived<T, 1>;
  
  static constexpr std::size_t children_size = 1ul << L;
  static constexpr std::size_t capacity = children_size;
  
  const children_type* children() const { return reinterpret_cast<const children_type*>(storage.get()); };
  
  node(std::size_t index, const T& value):
    base(std::make_shared<single_type>(1ul << index, value)) { }

  node(children_type children): base(std::move(children)) { }
  
  const T& get(std::size_t index) const {
    assert(index < children_size);
    assert(children()->contains(index));
    return children()->get(index);
  }

  node set(std::size_t index, const T& value) const {
    assert(index < children_size);
    return children()->set(index, value);
  }

  node<T, 1, B, L> raise() const { return {0, *this}; }
};



template<class T, std::size_t B, std::size_t L>
class array {
  const std::size_t level;
  const base data;
  
  // TODO compute from B, L
  static constexpr std::size_t max_level = 8;
  
  template<class Ret, std::size_t ... Ms, class Func>
  Ret visit(std::index_sequence<Ms...>, const Func& func) const {
    using thunk_type = Ret (*)(const base&, const Func& func);
    static const thunk_type table[] = {[](const base& data, const Func& func) -> Ret {
      return func(static_cast<const node<T, Ms, B, L>&>(data));
    }...};

    return table[level](data, func);
  }

  array(std::size_t level, const base data): level(level), data(data) { }
  
public:

  array(): level(0) { }

  const T& get(std::size_t index) const {
    assert(data.storage);
    return visit<const T&>(std::make_index_sequence<max_level>{}, [&](const auto& self) {
      return self.get(index);
    });
  }

  array set(std::size_t index, const T& value) const {
    // return visit<array>(std::make_index_sequence<max_level>{}, [&](const auto& self) {
    //   using node_type = decltype(self);

    //   if(data.storage) {
    //     if(index > node_type::capacity) {
    //       return array(level + 1, self.raise()).set(index, value);
    //     } else {
    //       return array(level, self.set(index, value));
    //     }
    //   } else {
    //     return array(level, 
    //   }
    // });
  }
  
  
};




#endif
