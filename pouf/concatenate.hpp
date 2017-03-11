#ifndef CONCATENATE_HPP
#define CONCATENATE_HPP

#include "sparse.hpp"

// concatenate incremental mapping matrix onto complete mapping matrix
// on source dofs
static void concatenate(rmat& res, const rmat& src) {
  res.resize(src.rows(), src.cols());

  // TODO externalize these guys
  std::vector<bool> mask(src.cols());
  std::vector<real> values(src.cols());
  std::vector<int> indices(src.cols());  

  
  // TODO better estimate? 
  const int estimated_nnz = src.nonZeros() + src.nonZeros() / 2;
  res.reserve(estimated_nnz);
  
  std::fill(mask.begin(), mask.end(), false);
  
  for(unsigned i = 0, n = src.rows(); i < n; ++i) {
    unsigned nnz = 0;
	
    for(rmat::InnerIterator src_it(src, i); src_it; ++src_it) {
      const real y = src_it.value();
      const unsigned k = src_it.col();

      // take row from previously computed if possible
      for(rmat::InnerIterator res_it(k < i ? res : src, k); res_it; ++res_it) {
        const unsigned j = res_it.col();
        const real x = res_it.value();

        if(!mask[j]) {
          mask[j] = true;
          values[j] = x * y;
          indices[nnz] = j;
          ++nnz;
        } else {
          values[j] += x * y;
        }
      }
    }

    std::sort(indices.data(), indices.data() + nnz);
    
    res.startVec(i);
    for(unsigned k = 0; k < nnz; ++k) {
      const unsigned j = indices[k];
      res.insertBack(i, j) = values[j];
      mask[j] = false;
    }
	
  }

  res.finalize();
}


#endif
