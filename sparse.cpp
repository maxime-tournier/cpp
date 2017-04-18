#include "timer.hpp"

#include <Eigen/SparseCore>

#include <random>
#include <iostream>

using real = double;
using cmat = Eigen::SparseMatrix<real, Eigen::ColMajor>;
using rmat = Eigen::SparseMatrix<real, Eigen::RowMajor>;
using triplet = Eigen::Triplet<real>;


static std::default_random_engine& engine() {
  static std::random_device dev;
  static std::default_random_engine res(dev());
  return res;
}

template<class T> struct uniform;

template<> struct uniform<int> {
  using type = std::uniform_int_distribution<int>;
};

template<> struct uniform<real> {
  using type = std::uniform_real_distribution<real>;
};


// sum duplicate triplets
template<class Iterator>
static Iterator merge_sorted_triplets(Iterator first, Iterator last) {
  struct stealer : triplet {
    using triplet::m_value;
  };
    
  return std::unique(first, last, [](triplet& lhs, const triplet& rhs) {
      const bool res = lhs.row() == rhs.row() && lhs.col() == rhs.col();
      if(res) static_cast<stealer&>(lhs).m_value += rhs.value();
      return res;
    });
    
}


namespace v1 {

  // reserve nnz directly + insertBack, std::sort + merge
  
  template<class Iterator>
  static void set_from_sorted_unique_triplets(rmat& matrix, Iterator first, Iterator last) {
    matrix.reserve(last - first);
    
    int last_row = -1;
    for(Iterator it = first; it != last; ++it) {

      while(last_row < it->row()) {
        matrix.startVec(++last_row);
      }

      matrix.insertBack(it->row(), it->col()) = it->value();
    }

    matrix.finalize();
  }



  
  
  template<class Iterator>
  static void set_from_triplets(rmat& matrix, Iterator first, Iterator last) {
    std::sort(first, last, [](const triplet& lhs, const triplet& rhs) {
        return lhs.row() < rhs.row() || (lhs.row() == rhs.row() && lhs.col() < rhs.col());
      });

    last = merge_sorted_triplets(first, last);
    set_from_sorted_unique_triplets(matrix, first, last);
  }
  
}


namespace v2 {
  // reserve nnz by rows + insertBackUncompressed, std::sort + merge
  
  
  template<class Iterator>
  static void set_from_sorted_unique_triplets(rmat& matrix, Iterator first, Iterator last,
                                              std::vector<rmat::StorageIndex>&& nnz = {}) {
    
    // reserve exactly nnz count per outer index
    nnz.clear(); nnz.resize(matrix.rows(), 0);
    
    for(Iterator it = first; it != last; ++it) {
      ++nnz[it->row()];
    }

    matrix.reserve(nnz);
    
    for(Iterator it = first; it != last; ++it) {
      matrix.insertBackUncompressed(it->row(), it->col()) = it->value();
    }
    
    matrix.finalize();
  }

  
 
  
  template<class Iterator>
  static void set_from_triplets(rmat& matrix, Iterator first, Iterator last) {
    std::sort(first, last, [](const triplet& lhs, const triplet& rhs) {
        return lhs.row() < rhs.row() || (lhs.row() == rhs.row() && lhs.col() < rhs.col());
      });

    last = merge_sorted_triplets(first, last);
    set_from_sorted_unique_triplets(matrix, first, last);
  }
  
}


namespace v3 {
  // counting sort + insertBackUncompressed

  
  template<class Triplets, class F>
  static void sort_triplets(std::size_t n, const F f,
                            const Triplets& input, Triplets& output,
                            std::vector<rmat::StorageIndex>&& count = {}) {
    // TODO memset?
    count.clear(); count.resize(n, 0);
    
    for(const triplet& t : input) {
      ++count[ f(t) ];
    }

    rmat::StorageIndex total = 0;
    for(rmat::StorageIndex& c : count) {
      rmat::StorageIndex old = c;
      c = total;
      total += old;
    }
    
    output.resize(input.size());
    for(const triplet& t : input ) {
      const rmat::StorageIndex at = count[ f(t) ]++;
      output[ at ] = t;
    }
  }


  static void set_from_triplets(rmat& matrix, std::vector<triplet>& input) {

    std::vector<triplet> tmp;

    std::vector<rmat::StorageIndex> count( std::max(matrix.rows(), matrix.cols()) );
    
    sort_triplets(matrix.cols(), [](const triplet& t) { return t.col(); }, input, tmp, std::move(count) );
    sort_triplets(matrix.rows(), [](const triplet& t) { return t.row(); }, tmp, input, std::move(count) );    
    
    auto last = merge_sorted_triplets(input.begin(), input.end());
    v2::set_from_sorted_unique_triplets(matrix, input.begin(), last, std::move(count));
  }
  

}


namespace v4 {
  // counting sort + insertBack
  
  static void set_from_triplets(rmat& matrix, std::vector<triplet>& input) {
    std::vector<triplet> tmp;

    std::vector<rmat::StorageIndex> count( std::max(matrix.rows(), matrix.cols()) );
    
    v3::sort_triplets(matrix.cols(), [](const triplet& t) { return t.col(); }, input, tmp, std::move(count) );
    v3::sort_triplets(matrix.rows(), [](const triplet& t) { return t.row(); }, tmp, input, std::move(count) );    
    
    auto last = merge_sorted_triplets(input.begin(), input.end());
    v1::set_from_sorted_unique_triplets(matrix, input.begin(), last);
  }
  

}


namespace v5 {
  // counting sort + custom insertion
  
  template<class Iterator>
  static void set_from_sorted_unique_triplets(rmat& matrix, Iterator first, Iterator last,
                                              std::vector<rmat::StorageIndex>&& positions = {}) {
    
    struct stealer : rmat {
      using rmat::rmat;
      using rmat::m_outerIndex;
      using rmat::m_data;
    };

    stealer dest(matrix.rows(), matrix.cols());
    
    // reset non-zeros
    const std::size_t n = dest.outerSize();

    // TODO use Eigen map to exploit alignment?
    std::fill(dest.m_outerIndex, dest.m_outerIndex + n, 0);
    
    // build histogram
    for(Iterator it = first; it != last; ++it) {
      ++dest.m_outerIndex[it->row()];
    }


    // prefix sum    
    rmat::StorageIndex count = 0;
    for(std::size_t i = 0; i < n; ++i) {
      const rmat::StorageIndex old = dest.m_outerIndex[i];
      dest.m_outerIndex[i] = count;
      count += old;
    }
    
    // sentinel?
    dest.m_outerIndex[n] = count;

    // alloc
    positions.resize(n);

    // TODO exploit alignment ?
    std::copy(dest.m_outerIndex, dest.m_outerIndex + n, positions.begin());
    
    // fill data
    dest.m_data.resize( count ); 
    for(Iterator it = first; it != last; ++it) {
      const rmat::StorageIndex pos = positions[it->row()]++;
      dest.m_data.index(pos) = it->col();
      dest.m_data.value(pos) = it->value();
    }
    
    // dest.finalize();
    dest.swap(matrix);
  }

  
  static void set_from_triplets(rmat& matrix, std::vector<triplet>& input) {

    std::vector<triplet> tmp;

    std::vector<rmat::StorageIndex> count( std::max(matrix.rows(), matrix.cols()) );
    
    v3::sort_triplets(matrix.cols(), [](const triplet& t) { return t.col(); }, input, tmp, std::move(count) );
    v3::sort_triplets(matrix.rows(), [](const triplet& t) { return t.row(); }, tmp, input, std::move(count) );    
    
    auto last = merge_sorted_triplets(input.begin(), input.end());
    set_from_sorted_unique_triplets(matrix, input.begin(), last, std::move(count) );
  }
  

}




int main(int, char**) {

  unsigned m = 5000, n = 1000;
  unsigned nnz = (m * n) / 100; 

  std::vector<triplet> values;

  uniform<int>::type m_rand(0, m - 1);
  uniform<int>::type n_rand(0, n - 1);
  uniform<real>::type value_rand(0, 1);  
  
  
  for(unsigned i = 0; i < nnz; ++i) {

    values.emplace_back(m_rand(engine()),
                        n_rand(engine()),
                        value_rand(engine()));
    
  }

  auto check = [&](const rmat& matrix) {
    // std::cerr << "check: " << matrix.sum() << std::endl;
  };
  

  auto ref = [&] {
    const real t = timer([&] {
        rmat A(m, n);
        std::vector<triplet> triplets = values;
        A.setFromTriplets(triplets.begin(), triplets.end());

        check(A);
      });

    
    std::clog << "eigen: " << t << std::endl;
  };
  

  auto v1 = [&] {
    const real t1 = timer([&] {
        rmat A(m, n);
        std::vector<triplet> triplets = values;
        v1::set_from_triplets(A, triplets.begin(), triplets.end());

        check(A);        
      });
    
    std::clog << "v1: " << t1 << std::endl;
  };
  

  auto v2 = [&] {
    const real t2 = timer([&] {
        rmat A(m, n);
        std::vector<triplet> triplets = values;
        v2::set_from_triplets(A, triplets.begin(), triplets.end());

        check(A);                
      });

    std::clog << "v2: " << t2 << std::endl;
  };

  auto v3 = [&] {
    const real t3 = timer([&] {
        rmat A(m, n);
        std::vector<triplet> triplets = values;
        v3::set_from_triplets(A, triplets);

        check(A);        
      });
 
    std::clog << "v3: " << t3 << std::endl;
  };


  auto v4 = [&] {
    const real t4 = timer([&] {
        rmat A(m, n);
        std::vector<triplet> triplets = values;
        v4::set_from_triplets(A, triplets);

        check(A);        
      });
 
    std::clog << "v4: " << t4 << std::endl;
  };


  static const auto v5 = [&] {
    const real t5 = timer([&] {
        rmat A(m, n);
        std::vector<triplet> triplets = values;
        v5::set_from_triplets(A, triplets);

        check(A);        
      });
 
    std::clog << "v5: " << t5 << std::endl;
  };
  
  auto warm = ref;
  
  warm();

  ref();
  v1();
  v2();
  v3();
  v4();
  v5();

  
  
  return 0;
}
 
