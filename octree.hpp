#ifndef CPP_OCTREE_HPP
#define CPP_OCTREE_HPP

#include <vector>
#include <bitset>

#include <iostream>

template<class T>
struct cell {
  static const std::size_t max_level = (8 * sizeof(T)) / 3;
  using coord = std::bitset<max_level>;
  
  static const std::size_t size = 3 * max_level;
  static const T resolution = 1 << max_level;
  
  using bits_type = std::bitset<size>;
  
  static const std::size_t padding = (8 * sizeof(T)) % 3;  
  bits_type bits;

  template<class Func>
  void decode(Func&& func) const {
    coord x, y, z;

    auto value = bits.to_ulong();
    for(std::size_t i = 0; i < max_level; ++i) {
      const std::size_t j = max_level - 1 - i;
      
      x[j] = value & 4;
      y[j] = value & 2;
      z[j] = value & 1;

      value >>= 3;
    }

    func(x, y, z);
  }

  static bits_type encode(const coord& x, const coord& y, const coord& z) {
    T value = 0;
    for(std::size_t i = 0; i < max_level; ++i) {
      value <<= 3;

      value |= x[i] * 4 + y[i] * 2 + z[i] * 1;
    }

    return value;
  }

  
  cell(const coord& x, const coord& y, const coord& z)
    : bits( encode(x, y, z) ) {
    
  }
    

  template<class Func>
  void neighbors(std::size_t level, Func&& func) const {
    const T low = 1 << level;
    const T up = 1 << (max_level - level);
    
    const T mask = ~(low - 1);
    
    static constexpr int delta [] = {-1, 0, 1};
    
    decode([&](coord x, coord y, coord z) {
        const T cx = x.to_ulong() & mask;
        const T cy = y.to_ulong() & mask;
        const T cz = z.to_ulong() & mask;

        for(int dx : delta) {
          if(cx < low && dx < 0) continue;
          if(cx > up && dx > 0) continue;
          for(int dy : delta) {
            if(cy < low && dy < 0) continue;
            if(cy > up && dy > 0) continue;            
            for(int dz : delta) {
              if(cz < low && dz < 0) continue;
              if(cz > up && dz > 0) continue;
              
              if(std::abs(dx) + std::abs(dy) + std::abs(dz)) {
                func(cx + dx * low, cy + dy * low, cz + dz * low);
              }
            }
          }
        }
      });
    
  }
  
};



template<class T>
class octree {
  std::vector<cell<T>> cells;
public:
  
};




#endif
