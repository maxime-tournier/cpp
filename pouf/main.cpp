
#include <core/stack.hpp>
#include <core/graph.hpp>

#include <core/func.hpp>
#include <core/metric.hpp>
#include <core/dofs.hpp>

#include <core/graph_data.hpp>

#include "typecheck.hpp"
#include "numbering.hpp"
#include "push.hpp"
#include "fetch.hpp"
#include "concatenate.hpp"

#include "simulation.hpp"

#include "uniform.hpp"
#include <core/rigid.hpp>

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






int main(int, char**) {
  rigid g;

  g.orient.setIdentity();
  
  std::cout << g.inv().coeffs() << std::endl;
  
  
  return 0;
}
 
