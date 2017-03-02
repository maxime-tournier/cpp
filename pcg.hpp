#ifndef PCG_HPP
#define PCG_HPP

#include <cstdint>
#include <cassert>

// http://www.pcg-random.org/download.html#minimal-c-implementation

class pcg32 {
  uint64_t state;  
  const uint64_t inc;

  static uint64_t seed() {
	static const uint64_t unique = 0;
	return (uint64_t)&unique;
  }
  
public:

  pcg32(uint64_t state,  // = 0x853c49e6748fea9bULL,
		uint64_t inc)// = 0xda3e39cb94b95bdbULL)
	: state(state),
	  inc(inc) {
	assert( inc % 2 == 1 );
  }

  pcg32(uint64_t state = seed())
	: state(state),
	  inc(2 * seed() + 1) {
  }
  
  uint32_t rand() {
	const uint64_t oldstate = state;
	// advance internal state
	state = oldstate * 6364136223846793005ULL + (inc|1);
	// calculate output function (XSH RR), uses old state for max ILP
	const uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	const uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }

  uint32_t rand(uint32_t bound) {
    const uint32_t threshold = -bound % bound;

	// eliminate bias by dropping output less than a threshold (RNG
	// range must be a multiple of bound)
    while(true) {
	  const uint32_t r = rand();
	  if (r >= threshold) {
		return r % bound;
	  }
    }
  }
  
};




#endif
