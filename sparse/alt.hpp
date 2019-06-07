#ifndef ALT_HPP
#define ALT_HPP

#include <memory>
#include <cassert>
#include <array>

namespace alt { 

namespace sparse {

  static std::size_t size(std::size_t mask) {
    return __builtin_popcountl(mask);
  }
  
  static std::size_t index(std::size_t mask, std::size_t index) {
    return size(mask & ((1ul << index) - 1));      
  }
  
}


template<class T>
struct base {
  const std::size_t mask;
  T data[0];
  
  base(std::size_t mask=0): mask(mask) { }

  std::size_t size() const { return sparse::size(mask); }
  T& at(std::size_t index) { return data[sparse::index(mask, index)]; }

  template<std::size_t M>
  std::shared_ptr<base> set(std::size_t index, T&& value) const;

  bool has(std::size_t index) const { return mask & (1ul << index); }
};


template<class T, std::size_t N>
struct derived: base<T> {
  T values[N];
  
  derived(std::size_t mask): base<T>(mask) { }
};


template<class Ret, std::size_t...Ms, class Cont>
static std::array<Ret, sizeof...(Ms)> unpack_sequence(std::index_sequence<Ms...>, Cont cont) {
  return {cont(std::integral_constant<std::size_t, Ms>{})...};
};

template<class T, std::size_t M>
static std::shared_ptr<base<T>> make_array(std::size_t mask) {
  using thunk_type = std::shared_ptr<base<T>> (*)(std::size_t mask);
  
  static const auto table =
    unpack_sequence<thunk_type>(std::make_index_sequence<M + 1>{}, [](auto index) -> thunk_type {
      using index_type = decltype(index);
      return +[](std::size_t mask) -> std::shared_ptr<base<T>> {
        return std::make_shared<derived<T, index_type::value>>(mask);
      };
    });
  
  return table[sparse::size(mask)](mask);
};



template<class T>
template<std::size_t M>
std::shared_ptr<base<T>> base<T>::set(std::size_t index, T&& value) const {
  const T* in = data;
  const std::size_t bit = 1ul << index;

  auto res = make_array<T, M>(mask | bit);

  const std::size_t s = sparse::index(res->mask, index);
  T* out = res->data;
  
  // copy leading values
  for(std::size_t i = 0; i < s; ++i) {
    *out++ = *in++;
  }

  // copy set value
  res->data[s] = std::move(value);

  // skip corresponding input if bit was set
  if(const bool skip = mask & bit) {
    ++in;
  }

  // copy trailing values
  for(std::size_t i = s + 1, n = res->size(); i < n; ++i) {
    *out++ = *in++;
  }

  return res;
}


template<class T, std::size_t level, std::size_t B, std::size_t L>
struct node {
  static_assert(level > 0, "level error");
  static_assert(L > 0, "L error");
  static_assert(B > 0, "B error");

  explicit operator bool() const { return bool(children); }
  
  static constexpr std::size_t max_size = 1ul << B;
  static_assert(max_size <= sizeof(std::size_t) * 8, "size error");
    
  static constexpr std::size_t shift = L + B * (level - 1ul);
  static constexpr std::size_t mask = (1ul << shift) - 1ul;

  struct split {
    const std::size_t sub;
    const std::size_t rest;
      
    split(std::size_t index):
      sub(index >> shift),
      rest(index & mask) {
      assert(sub < max_size);
    }
  };
  
  using child_type = node<T, level - 1, B, L>;
  using children_type = std::shared_ptr<base<child_type>>;
  children_type children;
  
  node(children_type children): children(std::move(children)) { }
  node() = default;
  
  T& get(split index) const {
    return children->at(index.sub).get(index.rest);
  }
  
  node set(split index, const T& value) const {
    if(children->has(index.sub)) {
      return children->template set<max_size>(index.sub, children->at(index.sub).set(index.rest, value));
    } else {
      return children->template set<max_size>(index.sub, child_type(index.rest, value));
    }
  }

  node(split index, const T& value):
    children(base<child_type>().template set<max_size>(index.sub, child_type(index.rest, value))) { }


  friend node emplace(node self, split index, const T& value) {
    if(!self.children.unique() || !self.children->has(index.sub)) {
      return self.set(index, value);
    }

    auto& child = self.children->at(index.sub);
    child = emplace(std::move(child), index.rest, value);
    
    return self;
  }
  

};


template<class T, std::size_t B, std::size_t L>
struct node<T, 0, B, L> {
  static constexpr std::size_t level = 0;
  static constexpr std::size_t max_size = 1ul << L;
  static_assert(max_size <= sizeof(std::size_t) * 8, "size error");
  
  using child_type = T;
  using children_type = std::shared_ptr<base<child_type>>;
  children_type children;
  
  node(children_type children): children(std::move(children)) { }
  node() = default;
  
  T& get(std::size_t index) const {
    return children->at(index);
  }

  node set(std::size_t index, const T& value) const {
    return children->template set<max_size>(index, T(value));
  }

  explicit operator bool() const { return bool(children); }

  node(std::size_t index, const T& value):
    children(base<child_type>().template set<max_size>(index, T(value))) { }

  friend node emplace(node self, std::size_t index, const T& value) {
    if(!self.children.unique() || !self.children->has(index)) {
      return self.set(index, value);
    }

    self.children->at(index) = value;
    
    return self;
  }
  
};


template<class T, std::size_t B=6, std::size_t L=6>
class amt {
  static constexpr std::size_t index_bits = sizeof(std::size_t) * 8;
  static constexpr std::size_t max_level = (index_bits - L) / B;

  using root_type = node<T, max_level, B, L>;
  root_type root;
  
  amt(root_type root): root(std::move(root)) { }
  
public:
  amt() : root(nullptr) {}

  amt set(std::size_t index, const T& value) const {
    if(!root) {
      return root_type(index, value);
    }

    return root.set(index, value);
  }

  const T& get(std::size_t index) const {
    return root.get(index);
  }

  friend amt emplace(amt self, std::size_t index, const T& value) {
    if(!self.root) return root_type(index, value);
    return emplace(std::move(self.root), index, value);
  }
  
};

}


#endif
