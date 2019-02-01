#ifndef RADIX_HPP
#define RADIX_HPP

#include <array>
#include <memory>
#include <cassert>

#include <iostream>

template<class T, std::size_t B, std::size_t L>
struct chunk;

static constexpr std::size_t chunk_ptr_size = 2 * sizeof(void*);
// ((1ul << B) * chunk_ptr_size) / sizeof(T) (TODO log2 of that)

template<class T, std::size_t B, std::size_t L=B>
struct chunk {
  using chunk_ptr_type = std::shared_ptr<chunk>;
  static_assert(sizeof(chunk_ptr_type) == chunk_ptr_size, "size error");
  
  static constexpr std::size_t children_size = 1ul << B;
  static constexpr std::size_t children_mask = children_size - 1;

  static constexpr std::size_t leaf_size = 1ul << L;
  static constexpr std::size_t leaf_mask = leaf_size - 1;
  
  using children_type = std::array<chunk_ptr_type, children_size>;
  using buffer_type = std::array<T, leaf_size>;
  
  static_assert(sizeof(buffer_type) <= sizeof(children_type), "size error");
  
  using storage_type = typename std::aligned_union<0, children_type, buffer_type>::type;
  storage_type storage;
  
  const std::size_t depth;
  const std::size_t shift;
  const std::size_t mask;
  
  buffer_type& buffer() {
    return reinterpret_cast<buffer_type&>(storage);
  }

  const buffer_type& buffer() const {
    return reinterpret_cast<const buffer_type&>(storage);
  }
  
  children_type& children() {
    return reinterpret_cast<children_type&>(storage);
  }

  const children_type& children() const {
    return reinterpret_cast<const children_type&>(storage);
  }

  
  chunk(std::size_t depth=0):
    depth(depth),
    shift(depth ? L + B * (depth - 1) : 0),
    mask(children_mask << shift) {
    
    if(depth) {
      new (&storage) children_type;
    } else {
      new (&storage) buffer_type;
    }
  }
  
  ~chunk() {
    if(depth) {
      children().~children_type();
    } else {
      buffer().~buffer_type();
    }
  }
  
  T& ref(std::size_t index) {
    assert(index < capacity());
    
    if(!depth) {
      assert(index < leaf_size);
      return buffer()[index];
    }
    
    // chop off leading chunk of B bits 
    const std::size_t sub = index >> shift;
    auto& c = children()[sub];
    
    if(!c) {
      c = std::make_shared<chunk>(depth - 1);
    }

    const std::size_t next = index & ~mask;
    return c->ref(next);
  }


  friend chunk_ptr_type try_emplace(chunk_ptr_type&& self, std::size_t index, const T& value) {
    assert(index < self->capacity());
    assert(self.unique());
    
    if(!self->depth) {
      // TODO could move
      self->buffer()[index] = value;
      return std::move(self);
    }
    
    const std::size_t sub = index >> self->shift;
    const std::size_t next = index & ~self->mask;
    
    if(auto& c = self->children()[sub]) {
      if(c.unique()) {
        // safe to emplace
        c = try_emplace(std::move(c), next, value);
      } else {
        // c is shared: cannot emplace
        c = c->set(next, value);
      }
      
      return std::move(self);
    } else {
      // no chunk: need to allocate/ref
      c = std::make_shared<chunk>(self->depth - 1);

      // TODO could move
      c->ref(next) = value;
      
      return std::move(self);
    }
  }

  
  chunk_ptr_type set(std::size_t index, const T& value) {
    assert(index < capacity());
    
    const chunk_ptr_type res = std::make_shared<chunk>(depth);

    // leaf chunk    
    if(!depth) {
      for(std::size_t i = 0; i < leaf_size; ++i) {
        res->buffer()[i] = (i == index) ? value : buffer()[i];
      }
      
      return res;
    }

    const std::size_t mask = children_mask << shift;
    const std::size_t next = index & ~mask;
    const std::size_t sub = (index & mask) >> shift;

    // inner chunk
    for(std::size_t i = 0; i < children_size; ++i) {
      if(i != sub) {
        res->children()[i] = children()[i];
        continue;
      }

      auto& r = res->children()[i];
      
      if(auto& c = children()[i]) {
        // delegate to existing child
        r = c->set(next, value);
      } else {
        // allocate fresh block and modify in-place
        r = std::make_shared<chunk>(depth - 1);
        r->ref(next) = value;
      }
    }
    
    return res;
  }

  std::size_t capacity() const {
    return 1 << (shift ? shift + B : L);
  }

  friend std::ostream& operator<<(std::ostream& out, const chunk& self) {
    out << "(";
    if(!self.depth) {
      bool first = true;
      for(const auto& it: self.buffer()) {
        if(first) first = false;
        else out << " ";
        out << it;
      }
    } else {
      bool first = true;
      for(const auto& it: self.children()) {
        if(first) first = false;
        else out << " ";
        
        if(it) out << *it;
        else out << "()";
      }
    }
    
    return out << ")";
  }
  
};




template<class T, std::size_t B=7, std::size_t L=B>
class vector {
  using chunk_type = chunk<T, B, L>;
  using root_type = typename chunk_type::chunk_ptr_type;
  
  root_type root;
  std::size_t _size;
  
protected:
  vector(const root_type& root, std::size_t size):
    root(root),
    _size(size) { }
public:

  std::size_t size() const { return _size; }
  
  vector():
    root(std::make_shared<chunk_type>()),
    _size(0) { };
  
  vector push_back(const T& value) const & {
    if(size() == root->capacity()) {
      root_type res = std::make_shared<chunk_type>(root->depth + 1);
      res->children()[0] = root;
      res->ref(size()) = value;
      return {res, size() + 1};
    }
    
    return {root->set(size(), value), size() + 1};
  }


  vector push_back(const T& value) && {
    if(size() == root->capacity()) {
      root_type res = std::make_shared<chunk_type>(root->depth + 1);
      res->children()[0] = std::move(root);
      res->ref(size()) = value;
      return {res, size() + 1};
    }

    if(root.unique()) {
      return {try_emplace(std::move(root), size(), value), size() + 1};
    } else {
      return {root->set(size(), value), size() + 1};
    }
  }

  
  const T& operator[](std::size_t index) const & {
    assert(index < root->capacity());
    assert(index < size());
    return root->get(index);
  }

  const T& get(std::size_t index) const & {
    assert(index < root->capacity());
    assert(index < size());
    return root->get(index);
  }
  
  vector set(std::size_t index, const T& value) const & {
    assert(index < root->capacity());
    assert(index < size());
    return {root->set(index, value), size()};
  }


  vector set(std::size_t index, const T& value) && {
    assert(index < root->capacity());
    assert(index < size());

    root->ref(index) = value;
    
    return {std::move(root), size()};
  }
  
  

  friend std::ostream& operator<<(std::ostream& out, const vector& self) {
    return out << *self.root;
  }
  
};

namespace alt {

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
    static constexpr std::size_t capacity =
             (shift + B >= 64) ? 0 : (1ul << (shift + B));

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

    friend ptr_type try_emplace(ptr_type&& self, std::size_t index, const T& value);
  
    ptr_type set(std::size_t index, const T& value) const {
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

    friend ptr_type try_emplace(ptr_type&& self, std::size_t index, const T& value) {
      assert(!capacity || index < capacity);
      assert(self.unique());

      const std::size_t sub = (index & mask) >> shift;
      const std::size_t next = index & ~mask;

      auto& c = self->children[sub];

      if(!c) {
        c = std::make_shared<child_type>();
        c->ref(next) = value;
        return std::move(self);
      }
    
      if(c.unique()) {
        c = try_emplace(std::move(c), next, value);
        return std::move(self);
      }
    
      // cannot emplace, fallback to set
      ptr_type res = std::make_shared<node>(self->children);
      res->children[sub] = res->children[sub]->set(next, value);

      return res;
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

    friend ptr_type try_emplace(ptr_type&& self, std::size_t index, const T& value) {
      assert(index < items_size);
      assert(self.unique());
    
      self->items[index] = value;
      return std::move(self);
    }

    template<class Func>
    void iter(const Func& func) const {
      for(const auto& it: items) {
        func(it);
      }
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
    std::shared_ptr<node_type<level>> root() const {
      return std::static_pointer_cast<node_type<level>>(ptr);
    }

  
    template<class Ret, class Func, class ... Args>
    Ret visit(const Func& func, Args&& ... args) const {
      // TODO function jump table?
      switch(level) {
      case 0: return func(root<0>(), std::forward<Args>(args)...);
      case 1: return func(root<1>(), std::forward<Args>(args)...);
      case 2: return func(root<2>(), std::forward<Args>(args)...);
      case 3: return func(root<3>(), std::forward<Args>(args)...);
      case 4: return func(root<4>(), std::forward<Args>(args)...);
      case 5: return func(root<5>(), std::forward<Args>(args)...);
      case 6: return func(root<6>(), std::forward<Args>(args)...);
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
                        const T& value,
                        bool emplace) const {
        if(size == node_type<level>::capacity) {
          auto root = std::make_shared<node_type<level + 1>>();
          root->children[0] = std::move(self);
          root->ref(size) = value;
          return {root, level + 1, size + 1};
        }

        if(emplace) {
          return {try_emplace(std::move(self), size, value), level, size + 1};
        } else {
          return {self->set(size, value), level, size + 1};
        }
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
      return visit<vector>(push_back_visitor(), size(), value);
    }

    vector push_back(const T& value) && {
      return visit<vector>(push_back_emplace_visitor(), size(), value, ptr.unique());
    }
  
  
    const T& operator[](std::size_t index) const & {
      assert(index < size());
      return visit<const T&>(get_visitor(), index);
    }

    template<class Func>
    void iter(const Func& func) const {
      return visit<void>(iter_visitor(), func);
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


}




#endif


