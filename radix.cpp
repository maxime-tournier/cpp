// -*- compile-command: "c++ -std=c++11 -o radix radix.cpp -O3 -DNDEBUG -march=native -lstdc++" -*-

#include "radix.hpp"

#include <vector>
#include <map>

template<class T>
static std::size_t fill_reference(std::size_t n) {
  using vec = std::vector<T>;

  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v.push_back(i);
  }
  
  return v.size();
}


template<class T>
static T fill_sum_reference(std::size_t n) {
  using vec = std::vector<T>;

  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v.push_back(i);
  }

  T sum = 0;
  for(auto& it: v) {
    sum += it;
  }
  
  return sum;
}





template<class T, std::size_t B, std::size_t L>
static std::size_t fill_push(std::size_t n) {
  using vec = vector<T, B, L>;
  
  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = v.push_back(i);
  }

  return v.size();
}

template<class T, std::size_t B, std::size_t L>
static std::size_t fill_emplace(std::size_t n) {
  using vec = vector<T, B, L>;
  
  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = std::move(v).push_back(i);
  }

  return v.size();
}


template<class T, std::size_t B, std::size_t L>
static T fill_sum_emplace(std::size_t n) {
  using vec = vector<T, B, L>;
  
  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = std::move(v).push_back(i);
  }

  T sum = 0;
  v.iter([&](const T& value) {
    sum += value;
  });
  
  return sum;
}


template<class T>
static T map_sum_ref(std::size_t n) {
  using map = std::map<std::size_t, T>;

  map m;

  for(std::size_t i = 0; i < n; ++i) {
    m.emplace(i, i);
  }

  T sum = 0;
  for(std::size_t i = 0; i < n; ++i) {
    sum += m[i];
  }

  return sum;
}


template<class T, std::size_t B>
static T map_sum_sparse_ref(std::size_t n) {
  using map = std::map<std::size_t, T>;

  map m;

  for(std::size_t i = 0; i < n; ++i) {
    m.emplace(B * i, i);
  }

  T sum = 0;
  for(std::size_t i = 0; i < n; ++i) {
    sum += m[B * i];
  }

  return sum;
}


template<class T, std::size_t B>
static T map_sum(std::size_t n) {

  map<T, B> m;

  for(std::size_t i = 0; i < n; ++i) {
    m = m.set(i, i);
  }

  T sum = 0;
  for(std::size_t i = 0; i < n; ++i) {
    sum += m.get(i);
  }

  return sum;
}


template<class T, std::size_t B>
static T map_sum_sparse(std::size_t n) {

  map<T, B> m;

  for(std::size_t i = 0; i < n; ++i) {
    m = m.set(B * i, i);
  }

  T sum = 0;
  for(std::size_t i = 0; i < n; ++i) {
    sum += m.get(B * i);
  }

  return sum;
}


template<class T, std::size_t B>
static T map_sum_emplace(std::size_t n) {

  map<T, B> m;

  for(std::size_t i = 0; i < n; ++i) {
    m = std::move(m).set(i, i);
  }

  T sum = 0;
  for(std::size_t i = 0; i < n; ++i) {
    sum += m.get(i);
  }

  return sum;
}



template<class T, std::size_t B>
static T map_sum_sparse_emplace(std::size_t n) {

  map<T, B> m;

  for(std::size_t i = 0; i < n; ++i) {
    m = std::move(m).set(B * i, i);
  }

  T sum = 0;
  for(std::size_t i = 0; i < n; ++i) {
    sum += m.get(B * i);
  }

  return sum;
}



int main(int, char**) {
  const std::size_t n = 2000000;
  // std::clog << fill_reference<double>(n) << std::endl;      
  // std::clog << fill_push<double, 8, 8>(n) << std::endl;
  
  // std::clog << fill_emplace<double, 8, 8>(n) << std::endl;

  // std::clog << fill_sum_reference<double>(n) << std::endl;
  // std::clog << fill_sum_emplace<double, 8, 8>(n) << std::endl;

  // std::clog << map_sum_ref<double>(n) << std::endl;
  // std::clog << map_sum<double, 8>(n) << std::endl;
  std::clog << map_sum_emplace<double, 8>(n) << std::endl;      

  // std::clog << map_sum_sparse_ref<double, 8>(n) << std::endl;  
  // std::clog << map_sum_sparse<double, 8>(n) << std::endl;
  // std::clog << map_sum_sparse_emplace<double, 8>(n) << std::endl;      
  
  return 0;
}
