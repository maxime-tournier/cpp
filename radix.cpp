// -*- compile-command: "c++ -std=c++11 -o radix radix.cpp -O3 -DNDEBUG -march=native -lstdc++" -*-

#include "radix.hpp"

#include <vector>

template<class T>
static std::size_t benchmark_reference(std::size_t n) {
  using vec = std::vector<T>;

  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v.push_back(i);
  }
  
  return v.size();
}



template<class T, std::size_t B, std::size_t L>
static std::size_t benchmark_emplace(std::size_t n) {
  using vec = vector<T, B, L>;

  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = std::move(v).push_back(i);
  }

  return v.size();
}


template<class T, std::size_t B, std::size_t L>
static std::size_t benchmark_push(std::size_t n) {
  using vec = vector<T, B, L>;

  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = v.push_back(i);
  }

  return v.size();
}


template<class T, std::size_t B, std::size_t L>
static std::size_t benchmark_push_alt(std::size_t n) {
  using vec = alt::vector<T, B, L>;
  
  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = v.push_back(i);
  }

  return v.size();
}

template<class T, std::size_t B, std::size_t L>
static std::size_t benchmark_emplace_alt(std::size_t n) {
  using vec = alt::vector<T, B, L>;
  
  vec v;

  for(std::size_t i = 0; i < n; ++i) {
    v = std::move(v).push_back(i);
  }

  return v.size();
}



int main(int, char**) {
  const std::size_t n = 40000000;
  // std::clog << benchmark_reference<double>(n) << std::endl;      
  // std::clog << benchmark_push<double, 8, 8>(n) << std::endl;
  // std::clog << benchmark_push_alt<double, 8, 8>(n) << std::endl;
  
  std::clog << benchmark_emplace<double, 8, 8>(n) << std::endl;
  // std::clog << benchmark_emplace_alt<double, 8, 8>(n) << std::endl;
  
  return 0;
}
