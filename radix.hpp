#ifndef RADIX_HPP
#define RADIX_HPP

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
  
  T& ref(std::size_t index) {
    assert(index < capacity());
    
    if(!depth) {
      assert(index < leaf_size);
      return buffer()[index];
    }
    
    // chop off leading chunk of B bits 
    const std::size_t mask = children_mask << shift;
    const std::size_t next = index & ~mask;
    const std::size_t sub = (index & mask) >> shift;
    
    auto& c = children()[sub];
    
    if(!c) {
      c = std::make_shared<chunk>(depth - 1);
    }
    
    return c->ref(next);
  }


  friend chunk_ptr_type try_emplace(const chunk_ptr_type& self, std::size_t index, const T& value) {
    assert(index < self->capacity());
    assert(self.unique());
    
    if(!self->depth) {
      // TODO could move
      self->buffer()[index] = value;
      return self;
    }

    const std::size_t mask = children_mask << self->shift;
    const std::size_t next = index & ~mask;
    const std::size_t sub = (index & mask) >> self->shift;

    if(auto& c = self->children()[sub]) {
      const chunk_ptr_type s = try_emplace(c, next, value);
      if(c == s) {
        // success
        return self;
      } else {
        // could not emplace, need to allocate
        const chunk_ptr_type res = std::make_shared<chunk>(self->depth - 1);
        for(std::size_t i = 0; i < children_size; ++i) {
          res->children()[i] = (i == sub) ? s : self->children()[i];
        }
        
        return res;
      }
    } else {
      // no chunk: allocate/ref
      c = std::make_shared<chunk>(self->depth - 1);

      // TODO could move
      c->ref(next) = value;

      return self;
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


template<class T, std::size_t B=7>
class vector {
  using chunk_type = chunk<T, B>;
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
      // TODO should we not move root at some point?
      return {try_emplace(root, size(), value), size() + 1};
    } else {
      return {root->set(size(), value), size() + 1};
    }
  }

  
  const T& operator[](std::size_t index) const {
    assert(index < root->capacity());
    assert(index < size());
    return root->get(index);
  }

  friend std::ostream& operator<<(std::ostream& out, const vector& self) {
    return out << *self.root;
  }
  
};





#endif


