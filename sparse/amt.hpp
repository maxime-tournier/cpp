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
    static constexpr std::size_t max_size = 1ul << B;
    static constexpr std::size_t shift = L + B * (level - 1ul);
    static constexpr std::size_t mask = (max_size - 1ul) << shift;
  
    using child_node = node<T, level - 1, B, L>;
    using children_type = sparse::array<child_node>;

    children_type children;

    struct split {
      const std::size_t sub;
      const std::size_t rest;
      
      split(std::size_t index):
        sub(index >> shift),
        rest(index & mask) {
        assert(!(sub & (max_size - 1ul)));
      }
    };
    
    
    static children_type make_single(split index, const T& value) {
      return children_type().set(index.sub, child_node(index.rest, value));
    }
  
    node(std::size_t index, const T& value):
      children(make_single(index, value)) { }
  
    node(children_type children):
      children(std::move(children)) { }
  
  
    const T& get(split index) const {
      assert(children.contains(index.sub));
      return children.get(index.sub).get(index.rest);
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
    
    
  };


  template<class T, std::size_t B, std::size_t L>
  struct node<T, 0, B, L> {
    static constexpr std::size_t max_size = 1ul << L;
   
    using children_type = sparse::array<T>;
    children_type children;

    node(std::size_t index, const T& value):
      children(children_type().set(1ul << index, value)) { 
    }
  
    node(children_type children):
      children(std::move(children)) { }

    node(node&&) = default;
    node(const node&) = default;  

    node& operator=(node&&) = default;
    node& operator=(const node&) = default;  

  
    const T& get(std::size_t index) const {
      return children.get(index);
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
    

    
  };



  template<class T, std::size_t B, std::size_t L>
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
  };


  template<class K>
  struct traits;

  template<class K, class V, std::size_t B, std::size_t L>
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
  };

  
  template<class T>
  struct traits<T*> {
    static std::size_t index(T* value) { return std::size_t(value); }
  };

  template<>
  struct traits<unsigned long> {
    static std::size_t index(std::size_t value) { return value; }
  };

  template<>
  struct traits<long> {
    static std::size_t index(long value) { return value; }
  };

  
  template<>
  struct traits<int> {
    static std::size_t index(int value) { return value; }
  };

  template<>
  struct traits<unsigned int> {
    static std::size_t index(unsigned int value) { return value; }
  };
  
  
}


#endif
