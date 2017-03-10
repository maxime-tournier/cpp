#ifndef GRAPH_DATA_HPP
#define GRAPH_DATA_HPP


#include "stack.hpp"


// TODO we probably want a tag phantom type here to get stricter
// typechecking
class graph_data {
  stack storage;
  std::vector<stack::frame> frame;
public:
  
  graph_data(std::size_t num_vertices,
             std::size_t capacity = 0)
    : storage(capacity),
      frame(num_vertices) {

  }

  
  template<class G>
  G* allocate(unsigned v, std::size_t count = 1) {
    return storage.allocate<G>(frame[v], count);
  }
  

  template<class G>
  G& get(unsigned v) {
    return storage.get<G>(frame[v]);
  }

  template<class G>
  const G& get(unsigned v) const {
    return storage.get<G>(frame[v]);    
  }


  std::size_t count(unsigned v) const {
    return frame[v].count;
  }


  // TODO reserve, size, etc
  void grow() {
    storage.grow();
  }

  void reset() {
    storage.reset();
  }
  
};




#endif
