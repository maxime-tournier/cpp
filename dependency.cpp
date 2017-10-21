// -*- compile-command: "c++ -std=c++11 -Wall -g dependency.cpp -o dependency -lstdc++ -lpthread" -*-

#include "graph.hpp"
#include "pool.hpp"

#include <set>
#include <map>
#include <algorithm>

template<class T>
struct vertex {
  using value_type = T;
  value_type value;
  
  // graph topology
  using ref = vertex*;
  ref first = 0, next = 0;

  // dfs
  mutable bool marked;

  // task pool
  std::mutex mutex;
  std::condition_variable cv;
  bool ready;

  // scheduling
  mutable float time;                   // completion time
  float duration = 1;                   // duration estimate
};


namespace graph {
  
  template<class T>
  struct traits< std::vector< vertex<T> > > {

    using graph_type = std::vector< vertex<T> >;

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
    }

    static void notify(graph_type& g, const ref_type& v) {
      {
        std::unique_lock<std::mutex> lock(v->mutex);
        v->ready = true;
      }
       
      v->cv.notify_all();
    }
    
    static void wait(graph_type& g, const ref_type& v) {
      std::unique_lock<std::mutex> lock(v->mutex);
      v->cv.wait(lock, [&] { return v->ready; });
    }

  };
  
}


// execute f on data dependency graph g through thread pool
template<class G, class Pool, class F>
static void exec(G& g, Pool& pool, const F& f) {
  using namespace graph;

  // final tasks
  std::set< ref_type<G> > roots;

  // ordered jobs by start time
  std::multimap<float, ref_type<G> > jobs;
  
  // compute roots/groups
  dfs_postfix(g, [&](const ref_type<G>& v) {
      roots.emplace(v);

      float start_time = 0;
      
      graph::iter(g, v, [&](const ref_type<G>& u) {
          roots.erase(u);
          start_time = std::max(start_time, u->time);
        });

      jobs.emplace(start_time, v);
      v->time = start_time + v->duration;
    });


  // schedule tasks by increasing start time
  for(const auto& j : jobs) {
    
    const ref_type<G>& v = j.second;
    
    // start task
    traits<G>::lock(g, v);
      
    pool.push( [&g, v, &f] {
        // wait for dependencies to finish
        graph::iter(g, v, [&](const ref_type<G>& u) {
            traits<G>::wait(g, u);
          });
          
        // perform computation
        f(v);
        
        // signal task
        traits<G>::notify(g, v);
      });
      
  }

  // wait for roots to complete
  for(const ref_type<G>& v : roots) {
    traits<G>::wait(g, v);
  }
  
}

#include <iostream>

static int fib(int n) {
  if(n < 2) return n;
  return fib(n-1) + fib(n-2);
}


int main(int, char**) {

  using vert = vertex<int>;

  using graph_type = std::vector<vert>;
  graph_type g(10);
  
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

  pool p;

  exec(g, p, [&](vert::ref v) {
      const std::size_t index = v - g.data();      
      pool::debug("task:", index);
      return fib(42);
    });

  exec(g, p, [&](vert::ref v) {
      const std::size_t index = v - g.data();      
      pool::debug("hi", index);
    });

  
  return 0;  
}
