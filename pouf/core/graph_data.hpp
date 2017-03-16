#ifndef GRAPH_DATA_HPP
#define GRAPH_DATA_HPP


#include "stack.hpp"
#include "slice.hpp"

// TODO we probably want a tag phantom type here to get stricter
// typechecking

// abstracts a data stack by managing an array of stack frames, one
// for each graph vertex

class graph_data {
  stack storage;
  std::vector<stack::frame> frame;
public:

  graph_data(std::size_t num_vertices = 0,
             std::size_t capacity = 0)
    : storage(capacity),
      frame(num_vertices) {

  }

  
  template<class G>
  slice<G> allocate(unsigned v, std::size_t count = 1) {
    assert(v < frame.size());
    return storage.allocate<G>(frame[v], count);
  }
  

  template<class G>
  slice<G> get(unsigned v) {
    assert(v < frame.size());
    return storage.get<G>(frame[v]);
  }

  template<class G>
  slice<const G> get(unsigned v) const {
    assert(v < frame.size());
    return storage.get<G>(frame[v]);    
  }


  std::size_t count(unsigned v) const {
    assert(v < frame.size());
    return frame[v].count;
  }


  // TODO reserve, size, etc
  void grow() {
    storage.grow();
  }


  void reset(std::size_t n) {
    frame.resize(n);
    storage.reset();
  }


  template<class G>
  slice<const G> peek() const {
    return storage.peek().template cast<const G>();
  }


  template<class G>
  slice<G> peek() {
    return storage.peek().template cast<G>();
  }

};





#endif
