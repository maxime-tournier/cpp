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
static U modular_inverse(U x, U n) {
  // note: x and n must be coprime
  U t_prev = 0, t = 1;
  
  U a = n, b = x;
  while(true) {
    const U q = a / b;
    const U r = a - b * q;
    if(!r) {
      assert(b == 1 && "arguments must be coprime");
      assert((x * t) % n == 1);
      return t;
    }
    
    // keep the modular inverse modulo n
    const U t_next = (t_prev + t * (n - q)) % n;
    
    t_prev = t;
    t = t_next;

    a = b;
    b = r;
  }
  
}


template<class U>
static U chinese_remainders(const U* a, const U* n, std::size_t size) {

  // let's be cautious
  using uint = unsigned long;
  using sint = long;

  // 
  const uint prod = std::accumulate(n, n + size, uint(1), std::multiplies<uint>());
  
  uint x = 0;

  for(std::size_t i = 0; i < size; ++i) {
    assert(a[i] < n[i]);

    // TODO assemble m modulo n[i] to avoid overflowing
    const uint m = prod / n[i];
    const uint ei = m * modular_inverse<uint>(m % n[i], n[i]);
    
    x += a[i] * ei;
    x %= prod;
  }

  // for(std::size_t i = 0; i < size; ++i) {
  //   assert( x % n[i] == a[i] );
  // }

  // for(std::size_t j = 0; j < prod; ++j) {
  //   uint y = x + j * size;
    
  //   std::clog << "j: " << j << " prod: " << prod << " y: " << y << std::endl;
    
  //   for(std::size_t i = 0; i < size; ++i) {
  //     uint q = j % n[i];
  //     std::clog << "  qi: " << q << std::endl;
  //     std::clog << "  test: " << ((a[i] + size * q) < n[i]) << std::endl;
  //     std::clog << "  test2: " << ((size * q) < n[i]) << std::endl;            
  //     assert( (y % n[i]) % size == a[i] );
  //   }

  // }
  
  
  return x;
}




#endif
