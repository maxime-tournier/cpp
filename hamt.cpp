#include "hamt.hpp"

#include <iostream>

#include <chrono>
#include <map>
#include <unordered_map>
#include <vector>


template<class Action, class Resolution = std::chrono::milliseconds,
		 class Clock = std::chrono::high_resolution_clock>
static double time(Action action) {
  typename Clock::time_point start = Clock::now();
  action();
  typename Clock::time_point stop = Clock::now();

  std::chrono::duration<double> res = stop - start;
  return res.count();
}




template<class T, std::size_t B, std::size_t L>
static void test_ordered(std::size_t size) {
  std::cout << "vector: " << time([=] {
    std::vector<T> values;
    for(std::size_t i = 0; i < size; ++i) {
      values.emplace_back(i);
    }
    
    return values.size();
  }) << std::endl;


  std::cout << "map: " << time([=] {
    std::map<std::size_t, T> values;
    for(std::size_t i = 0; i < size; ++i) {
      values.emplace(i, i);
    }
    
    return values.size();
  }) << std::endl;

  std::cout << "unordered_map: " << time([=] {
    std::unordered_map<std::size_t, T> values;
    for(std::size_t i = 0; i < size; ++i) {
      values.emplace(i, i);
    }
    
    return values.size();
  }) << std::endl;
  
  std::cout << "hamt: " << time([=] {
    hamt::array<T, B, L> values;
    for(std::size_t i = 0; i < size; ++i) {
      values = values.set(i, i);
    }
  }) << std::endl;


  std::cout << "hamt emplace: " << time([=] {
    hamt::array<T, B, L> values;
    for(std::size_t i = 0; i < size; ++i) {
      values = std::move(values).set(i, i);
    }
  }) << std::endl;
  
}



int main(int, char**) {
  //
  hamt::array<double, 5, 5> test;
  test = test.set(0, 1);
  
  std::clog << test.find(0) << std::endl;
  std::clog << test.find(1) << std::endl;

  test_ordered<double, 5, 4>(10000);


  hamt::map<void*, double, 5, 5> map;
  map = map.set(nullptr, 0);
  
  return 0;
}
