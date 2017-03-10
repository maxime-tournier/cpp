#include "types.hpp"
#include "stack.hpp"
#include "graph.hpp"

#include "func.hpp"
#include "metric.hpp"
#include "dofs.hpp"

#include "graph_data.hpp"

#include "typecheck.hpp"
#include "numbering.hpp"
#include "push.hpp"
#include "fetch.hpp"
#include "concatenate.hpp"


template<int M, int N, class U = real>
struct sum : func< U (vector<M, U>, vector<N, U> ) > {

  virtual std::size_t size(const vector<M, U>& lhs, const vector<N, U>& rhs) const {
    return 1;
  }
  
  
  virtual void apply(U& to, const vector<M, U>& lhs, const vector<N, U>& rhs) const {
    to = lhs.sum() + rhs.sum();
  }
  

  virtual void jacobian(triplet_iterator lhs_block, triplet_iterator rhs_block,			
                        const vector<M, U>& lhs, const vector<N, U>& rhs) const {

    for(int i = 0, n = lhs.size(); i < n; ++i) {
      *lhs_block++ = {0, i, 1.0};
    }
    
    for(int i = 0, n = rhs.size(); i < n; ++i) {
      *rhs_block++ = {0, i, 1.0};
    }

  }
  
};



template<class U>
struct norm2 : func< scalar<U> ( U ) > {
  
  virtual std::size_t size(const U& ) const {
    return 1;
  }
  
  
  virtual void apply(scalar<U>& to, const U& from) const {
    to = traits<U>::dot(from, from) / 2.0;
  }
  

  virtual void jacobian(triplet_iterator block, const U& from) const {
    for(int i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
      *block++ = {0, i, traits<U>::coord(i, from)};
    }
  }


  virtual void hessian(triplet_iterator block, const scalar<U>& lambda, const U& from) const {
    for(int i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
      *block++ = {i, i, lambda};
    }
  }
  
};


// use this for constraints
template<class U>
struct pairing : func< scalar<U>(U, U) > {
  virtual std::size_t size(const U& ) const {
    return 1;
  }

  virtual void apply(scalar<U>& to, const U& lhs, const U& rhs) const {
    to = traits<U>::dot(lhs, rhs);
  }


  virtual void jacobian(triplet_iterator lhs_block, triplet_iterator rhs_block,
                        const U& lhs, const U& rhs) const {
    for(unsigned i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
      *lhs_block++ = {0, i, traits<U>::coord(i, rhs)};
      *rhs_block++ = {0, i, traits<U>::coord(i, lhs)};
    }
  }


  virtual void hessian(triplet_iterator block, const scalar<U>& lambda,
                       const U& lhs, const U& rhs) const {
    for(int i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
      *block++ = {i, n + i, lambda};
      *block++ = {n + i, i, lambda};      
    }
    
  }
  
};





template<class F>
static void with_auto_stack( const F& f ) {
  while (true) {
    try{
      f();
      return;
    } catch(stack::overflow& e) {
      e.who.grow();
	  // std::clog << "stack reserve: " << e.who.capacity()
      //         << " -> " << e.size << std::endl;
      e.who.reset();
    }
  }
}




int main(int, char**) {

  graph g;

  // independent dofs
  auto point3 = g.add< dofs<vec3> >();
  auto point2 = g.add< dofs<vec2> >();

  auto mass3 = g.add< uniform<metric_kind::mass, vec3> >(2);
  auto mass2 = g.add< uniform<metric_kind::mass, vec2> >();  
  
  // mapped
  auto map1 = g.add< sum<3, 2> >();
  auto map2 = g.add< norm2<double> >();  
  
  auto ff1 = g.add< uniform<metric_kind::stiffness, double> >(3);
  
  point3->pos = {1, 2, 3};
  point2->pos = {4, 5};  
  
  add_edge(map1->vertex, point3->vertex, g);
  add_edge(map1->vertex, point2->vertex, g);

  add_edge(map2->vertex, map1->vertex, g);  
  
  add_edge(mass3->vertex, point3->vertex, g);
  add_edge(mass2->vertex, point2->vertex, g);

  add_edge(ff1->vertex, map2->vertex, g);
  
  std::vector<unsigned> order;
  g.sort(order);

  for(unsigned v : order) {
    g[v].apply( typecheck(), v, g );
  }

  const std::size_t n = num_vertices(g);
  
  graph_data pos(n);
  std::vector<chunk> chunks(n);

  // propagate positions and number dofs
  std::size_t offset;
  
  with_auto_stack([&] {
      offset = 0;
      for(unsigned v : order) {
        g[v].apply( push(pos), v, g);
        g[v].apply( numbering(chunks, offset, pos), v);	
      }
    });

  const std::size_t total_dim = offset;
  
  graph_data mask(n);
  
  std::vector<triplet> jacobian, diagonal;
  std::vector< std::vector<triplet> > elements;

  const real dt = 0.01;
  
  // compute masks/jacobians
  with_auto_stack([&] {
      jacobian.clear();
      diagonal.clear();

      fetch vis(jacobian, diagonal, mask, elements, chunks, pos);
      vis.dt = dt;
	  
      for(unsigned v : reverse(order)) {
        g[v].apply( vis, v, g);
      }
    });

  rmat J(total_dim, total_dim), H(total_dim, total_dim);
  
  J.setFromTriplets(jacobian.begin(), jacobian.end());
  H.setFromTriplets(diagonal.begin(), diagonal.end());  

  std::cout << "diagonal: " << H << std::endl;
  
  rmat concat;
  concatenate(concat, J);
  
  std::cout << "after concatenation: " << std::endl;
  std::cout << concat << std::endl;

  rmat full = concat.transpose() * H * concat;
  std::cout << "H: " << full << std::endl;
  
  return 0;
}
 
