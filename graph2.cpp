// -*- compile-command: "c++ -std=c++11 -Wall -g graph2.cpp -o graph2" -*-

#include <memory>
#include <stdexcept>
#include <vector>

template<class T>
struct vertex;

template<class T>
struct vertex {
  T value;

  using ref_type = vertex*;
  ref_type first = nullptr, next = nullptr;

  mutable bool marked;
  
  void connect(ref_type target) {
    // find target's last sibling
    ref_type* out = &target->next;
    while(*out) out = &(*out)->next;

    // plug our first child as target's last sibling
    *out = first;

    // update our first child
    first = target;
  }


  void disconnect(ref_type target) {
    ref_type* out = &first;
    while(*out != target ){
      if(!*out) throw std::runtime_error("invalid target vertex");      
      out = &(*out)->next;
    }

    *out = target->next;
    target->next = nullptr;
  }


};


// postfix depth-first traversal starting from v, calling f on each
// vertex. warning: assumes all the traversed vertices have been cleared.
template<class V, class F>
static void dfs_postfix(V v, const F& f) {
  v->marked = true;
  
  for(V u = v->first; u; u = u->next) {
    if(!u->marked) dfs_postfix(u, f);
  }

  f(v);
}

// postfix depth-first traversal for a graph given as a range of vertices
template<class Iterator, class F>
static void dfs_postfix(Iterator first, Iterator last, const F& f) {
  for(Iterator it = first; it != last; ++it) {
    it->marked = false;
  }
  
  for(Iterator it = first; it != last; ++it) {
    if(it->marked) continue;
    dfs_postfix(it, f);
  }
  
}


template<class Iterator, class Out>
static void topological_sort(Iterator first, Iterator last, Out out) {
  dfs(first, last, [&](Iterator v) {
      // every successor of v gets processed before v (prefix dfs), so every
      // node on which v depends will come *before* v in the ordering
      *out++ = v;
    });
}


#include <iostream>

int main(int, char**) {

  using vert = vertex<int>;
  
  std::array<vert, 10> v;

  v[0].connect(&v[1]);
  v[0].connect(&v[2]);

  v[1].connect(&v[3]);
  v[1].connect(&v[4]);  

  v[2].connect(&v[5]);
  v[2].connect(&v[6]);
  v[2].connect(&v[4]);    

  v[3].connect(&v[7]);
  v[3].connect(&v[8]);  

  v[4].connect(&v[9]);

  v[0].disconnect(&v[2]);

  const auto visitor = [&](vert::ref_type u) {
    std::clog << u - v.data() << std::endl;
  };


  std::vector< vert::ref_type > top_down;
  topological_sort(v.begin(), v.end(), std::back_inserter(top_down));

  for(auto v : top_down) {
    visitor(v);
  }
  
  return 0;  
}
