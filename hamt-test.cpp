#include <gtest/gtest.h>

#include <bitset>

#include "hamt.hpp"

TEST(hamt, iter) {
  const std::size_t indices[] = {
    0, 4, 15, 16, 24, 31
  };

  const double values[] = {
    0.0, 1.0, 2.0, 3.0, 4.0, 5.0
  };

  std::size_t mask = 0;
  for(auto i: indices) {
    mask |= 1ul << i;
  }
  
  sparse::storage<double, 32, 6> test(mask,
                                      values[0],
                                      values[1],
                                      values[2],
                                      values[3],
                                      values[4],
                                      values[5]);
  const std::size_t* ri = indices;
  const double* rj = values;
  
  test.iter(0, [&](std::size_t i, double j) {
    // std::cout << i << ": " << j << std::endl;
    ASSERT_EQ(i, *ri++);
    ASSERT_EQ(j, *rj++);    
  });

  hamt::array<double, 5, 4> x;
  x = x.set(1, 1.0);
  x = x.set(2, 2.0);
  x = x.set(16, 3.0);
  x = x.set(32, 32.0);
  x = x.set(47, 33.0);
  x = x.set(48, 33.0);          
  x = x.set(100000, 10.0);


  using traits = hamt::traits<5, 4>;
  auto show = [](auto indices) {
    std::size_t shift = 0;
    for(auto i: indices) {
      std::cout << i << "/" << (i << shift) << ", ";
      if(shift) shift += 5;
      else shift += 4;
    }
    std::cout << std::endl;
  };

  std::clog << std::bitset<64>(traits::masks[0]) << std::endl;
  std::clog << std::bitset<64>(traits::masks[1]) << std::endl;
  std::clog << std::bitset<64>(traits::masks[2]) << std::endl;    
  
  show(traits::split(47, traits::level_indices{}));
  show(traits::split(48, traits::level_indices{}));  
  std::clog << x.get(48) << std::endl;

  
  x.iter([](auto i, auto j) {
    std::clog << i << " " << j << std::endl;
  });

}
