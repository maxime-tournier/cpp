#ifndef CPP_GRAPH_HPP
#define CPP_GRAPH_HPP

#include <stdexcept>

// a generic dependency graph
namespace graph {

  template<class G>
  struct traits;

  // a reference to an abstract vertex type: it must allow the storing and
  // retrieval of informations given a graph and a reference.
  template<class G>
  using ref_type = typename traits<G>::ref_type;


  // iterate on graph vertices
  template<class G, class F>
  static void iter(G& g, const F& f) {
    traits<G>::iter(g, f);
  }


  // iterate on adjacent vertices
  template<class G, class F>
  static void iter(G& g, const ref_type<G>& v, const F& f) {
    for(ref_type<G> u = traits<G>::first(g, v); u; u = traits<G>::next(g, u)) {
      f(u);
    }
  }
  
  

  // add edge (source, target) to graph g with duplicate edge check.
  // O(out_degree(source))
  template<class G>
  static void connect(G& g, ref_type<G> source, ref_type<G> target) {
    // find source last child
    ref_type<G>& first = traits<G>::first(g, source);
    
    ref_type<G>* sib = &first;
    while(*sib) {
      if(*sib == target) throw std::runtime_error("duplicate edge");
      sib = &traits<G>::next(g, *sib);
    }
    
    // set to target
    *sib = target;
  }

  
  // remove edge (source, target) from graph g. O(out_degree(source))
  template<class G>
  static void disconnect(G& g, ref_type<G> source, ref_type<G> target) {
    // find target in source children
    ref_type<G>& first = traits<G>::first(g, source);
    ref_type<G>* sib = &first;
    
    while(*sib != target ) {
      if(!*sib) throw std::runtime_error("unknown edge");      
      sib = &traits<G>::next(g, *sib);
    }

    // plug hole
    ref_type<G>& next = traits<G>::next(g, target);    
    *sib = next;

    // update target
    traits<G>::reset(g, next);
  }

  
  namespace detail {
    // postfix depth-first traversal starting from v, calling f on each
    // vertex. WARNING: assumes all the traversed vertices have been cleared.
    template<class G, class V, class F>
    static void dfs_postfix(G& g, const V& v, const F& f) {
      traits<G>::mark(g, v, true);
      
      iter(g, v, [&](const V& u) {
          if(!traits<G>::marked(g, u)) {
            dfs_postfix(g, u, f);
          }
        });
      
      f(v);
    }


    // add edge (source, target) to graph g *without* duplicate edge check.
    // O(num_siblings(target))
    template<class G>
    static void connect_fast(G& g, ref_type<G> source, ref_type<G> target) {
      // find target's last sibling
      ref_type<G>& next = traits<G>::next(g, target);
    
      ref_type<G>* sib = &next;
      while(*sib) {
        sib = &traits<G>::next(g, *sib);
      }
    
      // connect our first child as target's last sibling
      ref_type<G>& first = traits<G>::first(g, source);
      *sib = first;
    
      // update our first child
      first = target;
    }
  }

  // postfix depth-first traversal for a graph given as a range of vertices
  template<class G, class F>
  static void dfs_postfix(G& g, const F& f) {

    // clear marks
    iter(g, [&](const ref_type<G>& v) {
        traits<G>::mark(g, v, false);
      });
    
    // dfs
    iter(g, [&](const ref_type<G>& v) {
        if(traits<G>::marked(g, v)) return;
        detail::dfs_postfix(g, v, f);
      });
    
  }

  template<class G, class Out>
  static void topological_sort(G& g, Out out) {
    dfs_postfix(g, [&](const ref_type<G>& v) {
        // every successor of v gets processed before v (prefix dfs), so every
        // node on which v depends will come *before* v in the ordering
        *out++ = v;
      });
  }


}



#endif
