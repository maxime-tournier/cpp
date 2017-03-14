#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <core/graph.hpp>
#include <core/stack.hpp>
#include <core/graph_data.hpp>

// TODO move these guys to visitor?
#include "typecheck.hpp"
#include "numbering.hpp"
#include "push.hpp"
#include "fetch.hpp"
#include "pull.hpp"
#include "select.hpp"

#include "concatenate.hpp"

#include <Eigen/SparseCholesky>

struct simulation {

  vec3 gravity = {0, -9.81, 0};
  
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
	
	// propagate positions
	graph_data pos(n);
	with_auto_stack([&] {
		for(unsigned v : order) {
		  g[v].apply( typecheck(), v, g);
		  g[v].apply( push(pos), v, g);
		}
	  });

	// TODO update graph according to collision detection
	// TODO reorder graph if it changed
	
	// number dofs
	std::vector<chunk> chunks(n);
	std::size_t offset;

	std::vector<triplet> primal, dual;
	std::size_t primal_offset, dual_offset;
	
	with_auto_stack([&] {
		offset = 0;
		primal_offset = 0;
		dual_offset = 0;		
		
		primal.clear();
		dual.clear();

		
		struct select vis{primal, dual, primal_offset, dual_offset, chunks};
		
		for(unsigned v : order) {
		  g[v].apply( numbering(chunks, offset, pos), v, g);

		  // TODO merge visitors?
		  g[v].apply(vis , v);
		}
	  });

	const std::size_t total_dim = offset;

	// primal dual selection matrices
	cmat P, Q;

	P.resize(total_dim, primal_offset);
	P.setFromTriplets(primal.begin(), primal.end());

	Q.resize(total_dim, dual_offset);	
	Q.setFromTriplets(dual.begin(), dual.end());

	
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
	rmat L(total_dim, total_dim), D(total_dim, total_dim);

	L.setFromTriplets(jacobian.begin(), jacobian.end());

	// pull gradients/geometric stiffness
	vec gradient, momentum;
	graph_data work(n);

	std::vector<triplet> gs;

	with_auto_stack([&] {
		
		gradient = vec::Zero(total_dim);
		momentum = vec::Zero(total_dim);
		
		gs.clear();
		
		pull vis(gradient, momentum, L, gs, work, pos, chunks, gravity);

		for(unsigned v : reverse(order)) {
		  g[v].apply(vis, v, g);
		}
		
	  });
	
	std::cout << "gradient: " << gradient.transpose() << std::endl;
	std::cout << "momentum: " << momentum.transpose() << std::endl;	
	
	// // add geometric stiffness TODO scale by dt
	// std::copy(gs.begin(), gs.end(), std::back_inserter(diagonal));
	
	D.setFromTriplets(diagonal.begin(), diagonal.end());  
	
	std::cout << "diagonal: " << D << std::endl;
	
	rmat concat;
	concatenate(concat, L);

	// collapse matrix on source dofs only
	// concat = concat * select;

	// TODO select compliant/master source dofs for easy matrix
	// splitting:

	// H = master.transpose() * K * master
	// J = compliant.transpose() * K * master
	// C = compliant.transpose() * K * compliant	
	
	std::cout << "mapping concatenation: " << std::endl;
	std::cout << concat << std::endl;

	const cmat LP = concat * P;
	const cmat LQ = concat * Q;
	
	const rmat H = LP.transpose() * D * LP;
	std::cout << "H: " << H << std::endl;

	vec rhs = vec::Zero(primal_offset + dual_offset);

	rhs.head(primal_offset) += LP.transpose() * momentum;
	rhs.head(primal_offset) -= dt * (LP.transpose() * gradient);

	std::cout << "rhs: " << rhs.head(primal_offset).transpose() << std::endl;
	
	if( dual_offset ) {
	  const rmat J = LQ.transpose() * D * LP;
	  const rmat C = LQ.transpose() * D * LQ;
	  
	  rhs.tail(dual_offset) -= (LQ.transpose() * gradient) / dt;
	  
	  std::cout << "J: " << J << std::endl;
	  std::cout << "C: " << C << std::endl;

	  std::cout << "rhs: " << rhs.tail(dual_offset).transpose() << std::endl;
	}
	  
	Eigen::SimplicialLDLT<cmat> inv;

	inv.compute(H.transpose());

	std::cout << "ldlt: " << (inv.info() == Eigen::Success) << std::endl;
	
  }
  

};



#endif
