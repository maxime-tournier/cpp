#ifndef SPARSE_HPP
#define SPARSE_HPP


#include <Eigen/Sparse>
#include "real.hpp"

using rmat = Eigen::SparseMatrix<real, Eigen::RowMajor>;
using cmat = Eigen::SparseMatrix<real, Eigen::ColMajor>;

using triplet = Eigen::Triplet<real>;
using triplet_iterator = std::back_insert_iterator< std::vector<triplet> >;


#endif
