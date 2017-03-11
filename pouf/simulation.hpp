#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include "graph.hpp"
#include "stack.hpp"

#include "graph_data.hpp"

#include "numbering.hpp"

#include <Eigen/SparseCholesky>

struct simulation {

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
  
  
  void step(graph& g, real dt) {

	std::vector<unsigned> order;
	g.sort( std::back_inserter(order) );

	const std::size_t n = num_vertices(g);
	
	
	// propagate positions and number dofs	  
	graph_data pos(n);
	std::vector<chunk> chunks(n);

	std::size_t offset;
  
	with_auto_stack([&] {
		offset = 0;
		for(unsigned v : order) {
		  g[v].apply( push(pos), v, g);
		  g[v].apply( numbering(chunks, offset, pos), v);	
		}
	  });

	const std::size_t total_dim = offset;
  
	// fetch masks/jacobians
	graph_data mask(n);
  
	std::vector<triplet> jacobian, diagonal;
	std::vector< std::vector<triplet> > elements;
	
	with_auto_stack([&] {
		jacobian.clear();
		diagonal.clear();
		
		fetch vis(jacobian, diagonal, mask, elements, chunks, pos);
		vis.dt = dt;
		
		for(unsigned v : reverse(order)) {
		  g[v].apply( vis, v, g);
		}
		
    });

	// concatenate/assemble
	rmat J(total_dim, total_dim), K(total_dim, total_dim);
	
	J.setFromTriplets(jacobian.begin(), jacobian.end());
	K.setFromTriplets(diagonal.begin(), diagonal.end());  
	
	std::cout << "diagonal: " << K << std::endl;
	
	rmat concat;
	concatenate(concat, J);

	// collapse matrix on source dofs only
	// concat = concat * select;

	// TODO select compliant/master source dofs for easy matrix
	// splitting:

	// H = master.transpose() * K * master
	// J = compliant.transpose() * K * master
	// C = compliant.transpose() * K * compliant	
	
	std::cout << "after concatenation: " << std::endl;
	std::cout << concat << std::endl;
	
	rmat H = concat.transpose() * K * concat;
	std::cout << "H: " << H << std::endl;

	Eigen::SimplicialLDLT<cmat> inv;

	inv.compute(H.transpose());

	std::cout << "ldlt: " << (inv.info() == Eigen::Success) << std::endl;
	
  }
  

};



#endif
