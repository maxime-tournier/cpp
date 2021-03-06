#ifndef SPARSE_AMT_HPP
#define SPARSE_AMT_HPP

#include "sparse.hpp"


namespace amt {
  
  template<class T, std::size_t ... Is, class Func>
  static constexpr std::array<T, sizeof...(Is)> unpack(std::index_sequence<Is...>,
                                                       const Func& func) {
    return {func(std::integral_constant<std::size_t, Is>{})...};
  }

  template<class T, std::size_t N, class Func>
  static constexpr std::array<T, N> unpack(const Func& func) {
    return unpack<T>(std::make_index_sequence<N>(), func);
  }


  template<class T, std::size_t level, std::size_t B, std::size_t L>
  struct node {
    static_assert(level > 0, "level error");
    static_assert(L > 0, "L error");
    static_assert(B > 0, "B error");
    
    static constexpr std::size_t max_size = 1ul << B;
    static_assert(max_size <= sizeof(std::size_t) * 8, "size error");
    
    static constexpr std::size_t shift = L + B * (level - 1ul);
    static constexpr std::size_t mask = (1ul << shift) - 1ul;
  
    using child_node = node<T, level - 1, B, L>;
    using children_type = sparse::array<child_node>;

    children_type children;

    struct split {
      const std::size_t sub;
      const std::size_t rest;
      
      split(std::size_t index):
        sub(index >> shift),
        rest(index & mask) {
        assert(sub < max_size);
      }
    };
    
    
    static children_type make_single(split index, const T& value) {
      return children_type().set(index.sub, child_node(index.rest, value));
    }
  
    node(std::size_t index, const T& value):
      children(make_single(index, value)) {
    }
  
    node(children_type children):
      children(std::move(children)) { }
  
  
    const T& get(split index) const {
      assert(children.contains(index.sub));
      return children.get(index.sub).get(index.rest);
    }


    const T* find(split index) const {
      if(children.contains(index.sub)) {
        return children.get(index.sub).find(index.rest);
      }

      return nullptr;      
    }

    
    node set(split index, const T& value) const {
      if(children.contains(index.sub)) {
        return children.set(index.sub, children.get(index.sub).set(index.rest, value));
      } else {
        return children.set(index.sub, child_node(index.rest, value));
      }
    }



    friend node emplace(node self, split index, const T& value) {
      if(!self.children.unique()) return self.set(index, value);

      // note: splitting inside function slightly increases perfs
      // TODO: investigate
      
      if(self.children.contains(index.sub)) {
        auto& child = self.children.get(index.sub);
        // since we're the only ref to children we may safely transfer their
        // ownership
        child = emplace(std::move(child), index.rest, value);
        return self;
      }
    
      return self.children.set(index.sub, child_node(index.rest, value));
    }


    template<class Cont>
    void iter(std::size_t offset, const Cont& cont) const {
      children.iter([&](std::size_t sub, const child_node& child) {
        child.iter(offset + (sub << shift), cont);
      });
    }
    
    
  };


  template<class T, std::size_t B, std::size_t L>
  struct node<T, 0, B, L> {
    static constexpr std::size_t level = 0;
    static constexpr std::size_t max_size = 1ul << L;
    static_assert(max_size <= sizeof(std::size_t) * 8, "size error");
    
    using children_type = sparse::array<T>;
    children_type children;

    node(std::size_t index, const T& value):
      children(children_type().set(index, value)) { }
  
    node(children_type children):
      children(std::move(children)) { }

    node(node&&) = default;
    node(const node&) = default;  

    node& operator=(node&&) = default;
    node& operator=(const node&) = default;  

  
    const T& get(std::size_t index) const {
      return children.get(index);
    }


    const T* find(std::size_t index) const {
      if(children.contains(index)) {
        return &children.get(index);
      }

      return nullptr;
    }
    
    node set(std::size_t index, const T& value) const {
      return children.set(index, value);
    }


    friend node emplace(node self, std::size_t index, const T& value) {
      if(!self.children.unique()) return self.set(index, value); 
      
      if(self.children.contains(index)) {
        self.children.get(index) = value;
        return self;
      }
      
      return self.children.set(index, value);
    }
    

    template<class Cont>
    void iter(std::size_t offset, const Cont& cont) const {
      children.iter([&](std::size_t index, const T& value) {
        cont(offset + index, value);
      });
    }
    
  };



  template<class T, std::size_t B=6, std::size_t L=6>
  class array {
    static constexpr std::size_t index_bits = sizeof(std::size_t) * 8;
    static constexpr std::size_t max_level = (index_bits - L) / B;

    using root_type = node<T, max_level, B, L>;
    root_type root;

  public:
    array(root_type root={{}}):
      root(std::move(root)) { }
  
    const T& get(std::size_t index) const {
      return root.get(index);
    }
  
    array set(std::size_t index, const T& value) const& {
       return root.set(index, value);
    }

    array set(std::size_t index, const T& value) && {
       // since we're a temporary we may safely transfer ownership
       return emplace(std::move(root), index, value);
    }

    template<class Cont>
    void iter(const Cont& cont) const {
      root.iter(0, cont);
    }

    const T* find(std::size_t index) const {
      return root.find(index);
    }
    
    
  };


  template<class K>
  struct traits;

  template<class K, class V, std::size_t B=6, std::size_t L=6>
  class map {
    using storage_type = array<V, B, L>;
    storage_type storage;
  public:
    map(storage_type storage={}): storage(std::move(storage)) { }
    
    const V& get(const K& key) const {
      return storage.get(traits<K>::index(key));
    }
    
    map set(const K& key, const V& value) const& {
       return storage.set(traits<K>::index(key), value);                                                  
    }

    map set(const K& key, const V& value) && {
       return std::move(storage).set(traits<K>::index(key), value);
    }

    const V* find(const K& key) const {
      return storage.find(traits<K>::index(key));
    }


    template<class Cont>
    void iter(const Cont& cont) const {
      storage.iter([&](std::size_t index, const K& value) {
        cont(traits<K>::key(index), value);
      });
    }
    
  };


  template<class T>
  struct traits {
    static std::size_t index(T self) { return std::size_t(self); }
    static T key(std::size_t self) { return T(self); }    
  };

  
}


#endif
