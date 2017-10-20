// -*- compile-command: "c++ -std=c++11 -Wall -g dependency.cpp -o dependency -lstdc++ -lpthread" -*-

#include "graph.hpp"
#include "pool.hpp"


template<class T>
struct vertex {
  T value;
  
  // graph topology
  using ref = vertex*;
  ref first = nullptr, next = nullptr;

  // dfs
  mutable bool marked;

  // task pool
  std::mutex mutex;
  std::condition_variable cv;
  bool ready;
};


namespace graph {
  
  template<class T, std::size_t N>
  struct traits< std::array< vertex<T>, N > > {

    using graph_type = std::array< vertex<T>, N >;

    using ref_type = typename vertex<T>::ref;
    
    static ref_type& next(graph_type& g, const ref_type& v) { return v->next; }
    static ref_type& first(graph_type& g, const ref_type& v) { return v->first; }    

    static void mark(graph_type& g, const ref_type& v, bool value) { v->marked = value; }
    static bool marked(graph_type& g, const ref_type& v) { return v->marked; }    
    
    template<class F>
    static void iter(graph_type& g, const F& f) {
      for(ref_type it = g.data(), end = g.data() + g.size(); it != end; ++it) {
        f(it);
      }
    }

    // concurrency traits
    static void lock(graph_type& g, const ref_type& v) {
      std::unique_lock<std::mutex> lock(v->mutex);
      v->ready = false;
      const std::size_t index = v - g.data();
    }

    static void notify(graph_type& g, const ref_type& v) {
      {
        std::unique_lock<std::mutex> lock(v->mutex);
        v->ready = true;
        const std::size_t index = v - g.data();
      }
       
      v->cv.notify_all();
    }
    
    static void wait(graph_type& g, const ref_type& v) {
      std::unique_lock<std::mutex> lock(v->mutex);
      const std::size_t index = v - g.data();
      v->cv.wait(lock, [&] { return v->ready; });
    }

  };
  
}


// execute f on data dependency graph g through thread pool
template<class G, class Pool, class F>
static void exec(G& g, Pool& pool, const F& f) {
  using namespace graph;
  
  dfs_postfix(g, [&](const ref_type<G>& v) {
      traits<G>::lock(g, v);
      
      pool.push( [&g, v, &f] {
          // wait for dependencies to finish
          for(ref_type<G> u = traits<G>::first(g, v); u; u = traits<G>::next(g, u)) {
            traits<G>::wait(g, u);
          }
          
          // perform computation
          f(v);
        
          // signal our future
          traits<G>::notify(g, v);
        });
      
    });

  pool.join();
}

#include <iostream>

int main(int, char**) {

  using vert = vertex<int>;
  
  std::array<vert, 10> g;
  
  graph::connect(g, &g[0], &g[1]);
  graph::connect(g, &g[0], &g[2]);
  
  graph::connect(g, &g[1], &g[3]);
  graph::connect(g, &g[1], &g[4]);
  
  graph::connect(g, &g[2], &g[5]);
  graph::connect(g, &g[2], &g[6]);
  graph::connect(g, &g[2], &g[4]);
  
  graph::connect(g, &g[3], &g[7]);
  graph::connect(g, &g[3], &g[8]);
  
  graph::connect(g, &g[4], &g[9]);
  
  graph::connect(g, &g[0], &g[3]);
  
  const auto visitor = [&](vert::ref u) {
    std::clog << u - g.data() << std::endl;
  };

  std::vector< vert::ref > top_down;
  graph::topological_sort(g, std::back_inserter(top_down));
  
  for(auto v : top_down) {
    visitor(v);
  }

  // compilation groups (TODO useful to remove mutexes in graph vertices?)
  std::vector<std::size_t> time(g.size(), 0);

  for(std::size_t i = 0, n = g.size(); i < n; ++i) {
    std::size_t t = 0;
    for(auto out = top_down[i]->first; out; out = out->next ) {
      std::size_t index = out - g.data();
      t = std::max(t, time[index]);
    }

    std::size_t index = top_down[i] - g.data();
    time[ index ] = t + 1;

    std::clog << "time[" << index << "] = " << time[index] << std::endl;
  }


  pool p(8);

  exec(g, p, [&](vert::ref v) {
      const std::size_t index = v - g.data();      
      for(int i = 0; i < 10; ++i) pool::debug("task:", index, i);
    });

  exec(g, p, [&](vert::ref v) {
      const std::size_t index = v - g.data();      
      pool::debug("hi", index);
    });

  
  return 0;  
}
