
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

#include "simulation.hpp"


#include "api.hpp"





// // use this for constraints
// template<class U>
// struct pairing : func< scalar<U>(U, U) > {
//   virtual std::size_t size(const U& ) const {
//     return 1;
//   }

//   virtual void apply(scalar<U>& to, const U& lhs, const U& rhs) const {
//     to = traits<U>::dot(lhs, rhs);
//   }


//   virtual void jacobian(triplet_iterator lhs_block, triplet_iterator rhs_block,
//                         const U& lhs, const U& rhs) const {
//     for(unsigned i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
//       *lhs_block++ = {0, i, traits<U>::coord(i, rhs)};
//       *rhs_block++ = {0, i, traits<U>::coord(i, lhs)};
//     }
//   }


//   virtual void hessian(triplet_iterator block, const scalar<U>& lambda,
//                        const U& lhs, const U& rhs) const {
//     for(int i = 0, n = traits< deriv<U> >::dim; i < n; ++i) {
//       *block++ = {i, n + i, lambda};
//       *block++ = {n + i, i, lambda};      
//     }
    
//   }
  
// };


template<class T>
struct derived;

struct base {
  virtual ~base() { }
  
  const api< derived<real>,
			 derived<vec3> > cast;
  
  template<class Derived>
  base(Derived* self)
	: cast(self, &base::cast) {
	
  }
  
};

template<class T>
struct derived : base {

  derived() : base(this) { }
  
};


struct visit {

  template<class T>
  void operator()(T* self) const {
	std::cout << typeid(T).name() << std::endl;
	std::cout << self << std::endl;
  }
};




int main(int, char**) {
  
  auto test = std::make_shared< derived<vec3> >();

  test->cast.apply(visit());
  std::cout << test.get() << std::endl;

  
  graph g;

  // independent dofs
  auto point3 = g.add_shared< static_dofs<vec3> >();
  auto point2 = g.add_shared< static_dofs<vec2> >();

  auto mass3 = g.add_shared< uniform_mass<vec3> >(2);
  auto mass2 = g.add_shared< uniform_mass<vec2> >();  
  
  // // mapped
  // auto map1 = g.add_shared< sum<3, 2> >();
  // auto map2 = g.add_shared< norm2<double> >();  
  
  auto ff1 = g.add_shared< uniform_stiffness<double> >(3);
  
  point3->pos[0] = {1, 2, 3};
  point2->pos[0] = {4, 5};  
  
  // g.connect(map1, point3);
  // g.connect(map1, point2);

  // g.connect(map2, map1);

  g.connect(mass3, point3);
  g.connect(mass2, point2);  

  // g.connect(ff1, map2);
  
  simulation sim;

  const real dt = 0.1;
  
  sim.step(g, dt);
  
  return 0;
}
 
