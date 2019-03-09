#ifndef RADIX_HPP
#define RADIX_HPP

#include <array>
#include <memory>
#include <cassert>

#include <iostream>


// inner nodes
template<class T, std::size_t level, std::size_t B, std::size_t L>
struct node {
  static_assert(level > 0, "specialization error");
  static_assert(B > 0, "branching error");
  static_assert(B * level + L <= 64, "level cannot be represented");
  
  static constexpr std::size_t children_size = 1ul << B;

  using ptr_type = std::shared_ptr<node>;
  
  using child_type = node<T, level - 1, B, L>;
  using child_ptr_type = std::shared_ptr<child_type>;
  
  using children_type = std::array<child_ptr_type, children_size>;
  children_type children;

  node(const children_type& children={}): children(children) { }
  
  static constexpr std::size_t shift = L + B * (level - 1);
  static constexpr std::size_t capacity = (shift + B >= 64) ? 0 : (1ul << (shift + B));

  static constexpr std::size_t mask = (children_size - 1ul) << shift;
  
  
  T& ref(std::size_t index) {
    assert(!capacity || index < capacity);

    const std::size_t sub = (index & mask) >> shift;
    auto& c = children[sub];

    if(!c) {
      c = std::make_shared<child_type>();
    }

    const std::size_t next = index & ~mask;
    return c->ref(next);
  }

  friend ptr_type try_emplace(ptr_type self, std::size_t index, const T& value);
  
  ptr_type set(std::size_t index, const T& value) const {
    assert(!capacity || index < capacity);
      
    const std::size_t sub = (index & mask) >> shift;
    const std::size_t next = index & ~mask;

    // TODO skip sub copy?
    ptr_type res = std::make_shared<node>(children);
    if(res->children[sub]) {
      res->children[sub] = res->children[sub]->set(next, value);
    } else {
      // allocating new child node, emplace
      res->children[sub] = try_emplace(std::make_shared<child_type>(), next, value);
    }

    return res;
  }

    
  friend ptr_type try_emplace(ptr_type self, std::size_t index, const T& value) {
    assert(self);
    assert(!capacity || index < capacity);
      
    if(!self.unique()) {
      return self->set(index, value);
    }

    // at this point we're the only ref to *self (but writes to a destructed
    // ref may not have completed yet so still potential data race :/ FIXME)
    const std::size_t sub = (index & mask) >> shift;
    const std::size_t next = index & ~mask;

    auto& c = self->children[sub];

    if(!c) {
      // allocate child + set ret
      c = std::make_shared<child_type>();
      c->ref(next) = value;
      return self;
    }

    // keep trying to emplace on child
    c = try_emplace(std::move(c), next, value);
    return self;
  }


  template<class Func>
  void iter(const Func& func) const {
    for(const auto& it: children) {
      if(!it) return;
      it->iter(func);
    }
  }
  
};


  
// leaf nodes
template<class T, std::size_t B, std::size_t L>
struct node<T, 0, B, L> {
  static_assert(L > 0, "branching error");

  static constexpr std::size_t capacity = 1ul << L;
  static constexpr std::size_t items_size = capacity;
  
  using items_type = std::array<T, items_size>;
  items_type items;
  
  T& ref(std::size_t index) {
    assert(index < items_size);
    return items[index];
  }

  using ptr_type = std::shared_ptr<node>;
  
  ptr_type set(std::size_t index, const T& value) const {
    assert(index < items_size);
    ptr_type res = std::make_shared<node>();

    for(std::size_t i = 0; i < items_size; ++i) {
      res->items[i] = (i == index) ? value : items[i];
    }

    return res;
  }

  friend ptr_type try_emplace(ptr_type self, std::size_t index, const T& value) {
    assert(self);
    assert(index < items_size);
      
    if(!self.unique()) {
      return self->set(index, value);
    }

    // at this point we're the only ref to *self (but writes to a destructed
    // ref may not have completed yet so still potential data race FIXME)
      
    self->items[index] = value;
    return self;
  }

  template<class Func>
  void iter(const Func& func) const {
    for(const auto& it: items) {
      func(it);
    }
  }

};


template<class T, std::size_t B=7, std::size_t L=B>
class map {
  template<std::size_t level>
  using node_type = node<T, level, B, L>;

  using ptr_type = std::shared_ptr<void>;
  ptr_type ptr;

  std::size_t level;

  template<std::size_t level>
  static std::shared_ptr<node_type<level>> cast(ptr_type ptr) {
    ptr_type local = std::move(ptr);
    return std::static_pointer_cast<node_type<level>>(local);
  }

  template<class Ret, class Func, class ... Args>
  static Ret visit(std::size_t level, ptr_type ptr,
                   const Func& func, Args&& ... args) {
    // TODO function jump table?
    switch(level) {
    case 0: return func(cast<0>(std::move(ptr)), std::forward<Args>(args)...);
    case 1: return func(cast<1>(std::move(ptr)), std::forward<Args>(args)...);
    case 2: return func(cast<2>(std::move(ptr)), std::forward<Args>(args)...);
    case 3: return func(cast<3>(std::move(ptr)), std::forward<Args>(args)...);
    case 4: return func(cast<4>(std::move(ptr)), std::forward<Args>(args)...);
    case 5: return func(cast<5>(std::move(ptr)), std::forward<Args>(args)...);
    case 6: return func(cast<6>(std::move(ptr)), std::forward<Args>(args)...);
      // case 7: return func(root<7>(), std::forward<Args>(args)...);
      // case 8: return func(root<8>(), std::forward<Args>(args)...);
      // case 9: return func(root<9>(), std::forward<Args>(args)...);
    default:
      throw std::logic_error("derp");
    }
  }


  struct set_visitor {
    template<std::size_t level>
    map operator()(std::shared_ptr<node_type<level>> self,
                   std::size_t index,
                   const T& value) const {
      if(index < node_type<level>::capacity) {
        return {self->set(index, value)};
      } else {
        auto next = std::make_shared<node_type<level + 1>>();
        next->children[0] = self;

        return {try_emplace(std::move(next), index, value)};
      }
    }
  };


  struct emplace_visitor {
    template<std::size_t level>
    map operator()(std::shared_ptr<node_type<level>> self,
                   std::size_t index,
                   const T& value) const {
      if(index < node_type<level>::capacity) {
        return {try_emplace(std::move(self), index, value)};
      } else {
        auto next = std::make_shared<node_type<level + 1>>();
        next->children[0] = self;

        return {try_emplace(std::move(next), index, value)};
      }
    }
  };

  
  struct get_visitor {
    template<std::size_t level>
    const T& operator()(std::shared_ptr<node_type<level>> self,
                        std::size_t index) const {
      assert(index < node_type<level>::capacity);
      return self->ref(index);
    }

  };
  

  template<std::size_t level>
  map(std::shared_ptr<node_type<level>> ptr): ptr(ptr), level(level) { }
  
public:

  map() { }

  // TODO rvalue overload
  map set(std::size_t index, const T& value) const & {
    if(!ptr) {
      auto ptr = std::make_shared<node_type<0>>();
      return map(ptr).set(index, value);
    }

    return map::visit<map>(level, ptr, set_visitor(), index, value);
  }

  map set(std::size_t index, const T& value) && {
    if(!ptr) {
      auto ptr = std::make_shared<node_type<0>>();
      return map(ptr).set(index, value);
    }

    return map::visit<map>(level, std::move(ptr), emplace_visitor(), index, value);
  }

  
  const T& get(std::size_t index) const {
    assert(ptr);
    return map::visit<const T&>(level, ptr, get_visitor(), index);
  }
  
};

  
template<class T, std::size_t B=7, std::size_t L=B>
class vector {
  static_assert((64 - L) % B == 0, "last level is not full");
  
  template<std::size_t level>
  using node_type = node<T, level, B, L>;

  using ptr_type = std::shared_ptr<void>;
  ptr_type ptr;

  std::size_t level;
  std::size_t count;

  template<std::size_t level>
  static std::shared_ptr<node_type<level>> cast(ptr_type ptr) {
    ptr_type local = std::move(ptr);

    // note: this takes a const ref
    return std::static_pointer_cast<node_type<level>>(local);
  }

  
  template<class Ret, class Func, class ... Args>
  static Ret visit(std::size_t level, ptr_type ptr,
                   const Func& func, Args&& ... args) {
    // TODO function jump table?
    switch(level) {
    case 0: return func(cast<0>(std::move(ptr)), std::forward<Args>(args)...);
    case 1: return func(cast<1>(std::move(ptr)), std::forward<Args>(args)...);
    case 2: return func(cast<2>(std::move(ptr)), std::forward<Args>(args)...);
    case 3: return func(cast<3>(std::move(ptr)), std::forward<Args>(args)...);
    case 4: return func(cast<4>(std::move(ptr)), std::forward<Args>(args)...);
    case 5: return func(cast<5>(std::move(ptr)), std::forward<Args>(args)...);
    case 6: return func(cast<6>(std::move(ptr)), std::forward<Args>(args)...);
      // case 7: return func(root<7>(), std::forward<Args>(args)...);
      // case 8: return func(root<8>(), std::forward<Args>(args)...);
      // case 9: return func(root<9>(), std::forward<Args>(args)...);
    default:
      throw std::logic_error("derp");
    }
  }

  
  struct push_back_visitor {
    template<std::size_t level>
    vector operator()(std::shared_ptr<node_type<level>> self,
                      std::size_t size,
                      const T& value) const {
      if(size == node_type<level>::capacity) {
        auto root = std::make_shared<node_type<level + 1>>();
        root->children[0] = std::move(self);
        root->ref(size) = value;
        return {root, level + 1, size + 1};
      }

      return {self->set(size, value), level, size + 1};
    }
      
  };

  
  struct push_back_emplace_visitor {
    template<std::size_t level>
    vector operator()(std::shared_ptr<node_type<level>> self,
                      std::size_t size,
                      const T& value) const {
      if(size == node_type<level>::capacity) {
        // need to allocate a new level
        auto root = std::make_shared<node_type<level + 1>>();
        root->children[0] = std::move(self);
        root->ref(size) = value;
        return {root, level + 1, size + 1};
      }

      return {try_emplace(std::move(self), size, value), level, size + 1};
    }
      
  };

  
  struct get_visitor {
    template<std::size_t level>
    const T& operator()(std::shared_ptr<node_type<level>> self,
                        std::size_t index) const {
      return self->ref(index);
    }
  };


  struct iter_visitor {
    template<std::size_t level, class Func>
    void operator()(std::shared_ptr<node_type<level>> self,
                    const Func& func) const {
      return self->iter(func);
    }
  };
  
  vector(ptr_type ptr, std::size_t level, std::size_t count):
    ptr(std::move(ptr)),
    level(level),
    count(count) { }
  
public:

  std::size_t size() const { return count; }
  
  vector():
    ptr(std::make_shared<node_type<0>>()),
    level(0),
    count(0) { };
  
  vector push_back(const T& value) const & {
    return visit<vector>(level, ptr,
                         push_back_visitor(), size(), value);
  }

  vector push_back(const T& value) && {
    return visit<vector>(level, std::move(ptr),
                         push_back_emplace_visitor(), size(), value);
  }
  
  
  const T& operator[](std::size_t index) const & {
    assert(index < size());
    return visit<const T&>(level, ptr, get_visitor(), index);
  }

  template<class Func>
  void iter(const Func& func) const {
    return visit<void>(level, ptr, iter_visitor(), func);
  }
  
  // const T& get(std::size_t index) const & {
  //   assert(index < root->capacity());
  //   assert(index < size());
  //   return root->get(index);
  // }
  
  // vector set(std::size_t index, const T& value) const & {
  //   assert(index < root->capacity());
  //   assert(index < size());
  //   return {root->set(index, value), size()};
  // }


  // vector set(std::size_t index, const T& value) && {
  //   assert(index < root->capacity());
  //   assert(index < size());

  //   root->ref(index) = value;
    
  //   return {std::move(root), size()};
  // }
  
  

  // friend std::ostream& operator<<(std::ostream& out, const vector& self) {
  //   return out << *self.root;
  // }
  
};




#endif


