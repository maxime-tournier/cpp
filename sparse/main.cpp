#include <iostream>

#include "amt.hpp"
#include "timer.hpp"

#include <vector>
#include <map>
#include <unordered_map>


template<std::size_t B, std::size_t L>
static double fill_amt(std::size_t n) {
  amt::array<double, B, L> res;

  for(std::size_t i = 0; i < n; ++i) {
    res = res.set(i, i);
  }

  return res.get(0);
}


template<std::size_t B, std::size_t L>
static double fill_amt_emplace(std::size_t n) {
  amt::array<double, B, L> res;

  for(std::size_t i = 0; i < n; ++i) {
    res = std::move(res).set(i, i);
  }

  return res.get(0);
}


static double fill_vector(std::size_t n) {
  std::vector<double> res;
  
  for(std::size_t i = 0; i < n; ++i) {
    res.emplace_back(i);
  }
  
  return res[0];
}


static double fill_map(std::size_t n) {
  std::map<std::size_t, double> res;
  
  for(std::size_t i = 0; i < n; ++i) {
    res.emplace(i, n);
  }
  
  return res[0];
}


static double fill_unordered_map(std::size_t n) {
  std::unordered_map<std::size_t, double> res;
  
  for(std::size_t i = 0; i < n; ++i) {
    res.emplace(i, n);
  }
  
  return res[0];
}





int main(int, char**) {

  // sparse array test
  sparse::array<double> test;

  test = test.set(1, 2.0);
  test = test.set(10, 4.0);
  test = test.set(20, 8.0);  

  test.iter([](std::size_t index, double value) {
    std::cout << index << ": " << value << std::endl;
  });
  
  std::clog << test.get(0) << std::endl;
  std::clog << test.get(10) << std::endl;
  std::clog << test.size() << std::endl;


  // array mapped trie
  std::size_t n = 1000000;
  
  // static constexpr std::size_t B = 12;
  // static constexpr std::size_t L = 16;

  static constexpr std::size_t B = 6;
  static constexpr std::size_t L = 6;

  amt::array<std::size_t, B, L> v;
  for(std::size_t i = 0; i < 1ul << 13; ++i) {
    v = std::move(v).set(i, i);
  }

  v.iter([](std::size_t i, std::size_t j) {
    std::clog << i << ": " << j << std::endl;
  });
  
  
  timer t;
  
  std::clog << "amt: " << (t.restart(), fill_amt<B, L>(n), t.restart()) << std::endl;
  std::clog << "std::map: " << (t.restart(), fill_map(n), t.restart()) << std::endl;
  std::clog << "std::unordered_map: " << (t.restart(), fill_unordered_map(n), t.restart()) << std::endl;      

  std::clog << "amt emplace: " << (t.restart(), fill_amt_emplace<B, L>(n), t.restart()) << std::endl;  
  std::clog << "std::vector: " << (t.restart(), fill_vector(n), t.restart()) << std::endl;
  
  return 0;
}

