

#include <iostream>

static const struct test {
  test() {
    auto x = make_array<double, 32>(0ul);
    x = x->set<32>(0, 1);
    x = x->set<32>(4, 2);
    std::clog << x->at(0) << std::endl;
    std::clog << x->at(4) << std::endl;    

    amt<double> test;

    for(std::size_t i = 0; i < (1ul << 14); ++i) {
      test = emplace(std::move(test), i, i);
    }

    std::clog << test.get(1ul << 10) << std::endl;
    
    std::exit(0);
    
  }
} instance;


