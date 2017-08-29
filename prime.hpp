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


  static value_type is_prime(value_type x) {
    if (x < 2) return false;
    
    for (value_type i = 2; true; ++i) {
      const std::size_t q = x / i;
      if (q < i) return true;
      if (x % i == 0) return false;
    }
    return true;
  }
  
  value_type operator()() {
    
    while(true) {
      ++current;
      
      bool has_div = false;
      
      for(value_type p : previous) {
        const value_type q = current / p;

        // simpler sqrt check
        if( q < p ) break;

        // dont use modulo
        if(current == q * p) {
          has_div = true;
          break;
        }
        
      }
      
      if(!has_div) {
        previous.emplace_back(current);
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

  // let's be cautious
  using uint = unsigned long;
  using sint = long;
  
  static const auto modular_inverse = [] (uint a, uint b) {
    // return a * v, where 0 <= v = inv(a) mod b
    
    const uint prod = a * b;
    sint e_prev = a, e = 0;
    
    while(uint r = a % b) {
      const uint q = a / b;
      
      // TODO fix case when < 0?
      const sint e_next = e_prev - q * e;
      
      e_prev = e;
      e = e_next;
    
      a = b;
      b = r;
    }

    if(e < 0) e += prod;
    assert(e >= 0);
    return e;
  };

  
  const uint prod = std::accumulate(n, n + size, uint(1), std::multiplies<uint>());
  
  uint x = 0;

  for(std::size_t i = 0; i < size; ++i) {
    assert(a[i] < n[i]);
    const uint m = prod / n[i];
    x += a[i] * modular_inverse(m, n[i]);
  }

  return x;
}




#endif
