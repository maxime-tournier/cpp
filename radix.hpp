#ifndef RADIX_HPP
#define RADIX_HPP

template<class T, std::size_t B, std::size_t L>
struct chunk;

static constexpr std::size_t chunk_ptr_size = sizeof(void*);

template<class T, std::size_t B, std::size_t L=((1 << B) * chunk_ptr_size) / sizeof(T)>
struct chunk {
  using chunk_ptr_type = std::shared_ptr<chunk>;
  static_assert(sizeof(chunk_ptr_type) == sizeof(void*), "size error");
  
  static constexpr std::size_t children_size = 1 << B;
  static constexpr std::size_t children_mask = children_size - 1;

  static constexpr std::size_t leaf_size = 1 << L;
  static constexpr std::size_t leaf_mask = leaf_size - 1;
  
  using children_type = std::array<chunk_ptr_type, children_size>;
  using buffer_type = std::array<T, leaf_size>;
  
  static_assert(sizeof(buffer_type) <= sizeof(children_type), "size error");
  
  using storage_type = typename std::aliagned_union<children_type, buffer_type>::type;
  const storage_type storage;
  
  const std::size_t depth;
  const std::size_t shift;
  
  buffer_type& buffer() {
    return reinterpret_cast<buffer_type&>(storage);
  }

  children_type& children() {
    return reinterpret_cast<children_type&>(storage);
  }

  chunk(std::size_t depth=0):
    depth(depth),
    shift(depth ? L + B * (depth - 1) : 0){
    
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
  
  T& get(std::size_t index) {
    assert(index < capacity());
    
    if(!depth) {
      assert(index < leaf_size);
      return buffer()[index];
    }
    assert(index >= leaf_size);
    
    // chop off leading chunk of B bits 
    const std::size_t mask = children_mask << shift;
    const std::size_t next = index & ~mask;
    const std::size_t sub = (index & mask) >> shift;
    
    auto& c = children()[sub];
    
    if(!c) {
      c = std::make_shared<chunk>(depth - 1);
    }
    
    return c->get(next);
  }

  chunk_ptr_type set(std::size_t index, const T& value) const {
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
        r.get(next) = value;
      }
    }
    
    return res;
  }

  std::size_t capacity() const {
    return 1 << (shift ? shift + B : L);
  }
  
};


template<class T, std::size_t B=7>
struct vector {
  using chunk_type = chunk<T, B>;
  using root_type = typename chunk_type::chunk_ptr_type;

  const root_type root;
  const std::size_t size;

protected:
  vector(const root_type& root, std::size_t size):
    root(root),
    size(size) { }
public:
  
  vector(): root(std::make_shared<chunk_type>()) { };
  
  template<class T>
  vector push_back(const T& value) const {
    if(size == root->capacity()) {
      root_type res = new chunk_type(root->depth + 1);
      res->children()[0] = root;
      res->get(size) = value;
      return res;
    }
    
    return {root->set(size, value), size + 1};
  }

  const T& operator[](std::size_t index) const {
    assert(index < root->capacity());
    assert(index < size);
    return root->get(index);
  }

  
  
};





#endif


