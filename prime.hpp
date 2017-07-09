#ifndef PRIME_HPP
#define PRIME_HPP

#include <cmath>
#include <numeric>

template<class U>
class prime_enumerator {
  using value_type = U;
  value_type current = 1;
  std::vector<value_type> previous;
public:
  
  value_type operator()() {
    
    while(true) {
      ++current;
      const value_type sqrt = std::sqrt(current);
      
      bool has_div = false;
      for(value_type p : previous) {
        if(current % p == 0) {
          has_div = true;
          break;
        }
        
        if( p > sqrt ) break;
      }
      
      if(!has_div) {
        previous.push_back(current);
        return current;
      }
    }
  }
};


template<class U>
static U gcd(U a, U b) {
  // vanilla euclid's algorithm
  U res;
  
  while(U r = a % b) {
    a = b;
    b = r;
    res = r;
  }

  return res;
}


template<class U, class T>
static void bezout(U a, U b, T* s, T* t) {
  // bezout decomposition
  T s_prev = 1, t_prev = 0;
  T s_curr = 0, t_curr = 1;
  
  while(U r = a % b) {
    const U q = a / b;

    const T s_next = s_prev - q * s_curr;
    const T t_next = t_prev - q * t_curr;    

    s_prev = s_curr;
    s_curr = s_next;
    
    t_prev = t_curr;
    t_curr = t_next;    

    a = b;
    b = r;
  }

  *s = s_curr;
  *t = t_curr;
}


template<class U>
static U chinese_remainders(const U* a, const U* n, std::size_t size) {

  static const auto modular_inverse = [] (U a, U b) {
    // return a * v, where 0 <= v = inv(a) mod b
  
    const U prod = a * b;
    long e_prev = a, e = 0;
  
    while(U r = a % b) {
      const U q = a / b;
    
      const long e_next = e_prev - q * e;
    
      e_prev = e;
      e = e_next;
    
      a = b;
      b = r;
    }

    if(e < 0) e += prod;
    return e;
  };

  
  const U prod = std::accumulate(n, n + size, 1, std::multiplies<U>());
  
  U x = 0;

  for(std::size_t i = 0; i < size; ++i) {
    const U m = prod / n[i];
    x += a[i] * modular_inverse(m, n[i]);
  }

  return x;
}




#endif
