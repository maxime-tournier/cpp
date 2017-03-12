#ifndef SLICE_HPP
#define SLICE_HPP

#include <cassert>

template<class G>
class slice {
protected:
  G* first;
  G* last;
public:
  slice(G* first, G* last)
	: first(first),
	  last(last) {
	assert(last > first);
  }
  
  G* begin() { return first; }
  G* end() { return last; }  

  const G* begin() const { return first; }
  const G* end() const { return last; }  

  std::size_t size() const { return last - first; }
  
  G& operator[](std::size_t index) { return first[index]; }
  const G& operator[](std::size_t index) const { return first[index]; }  

  template<class T>
  slice& operator=( const std::initializer_list<T>& data ) {
	std::copy(data.begin(), data.end(), begin());
	return *this;
  }

  template<class U>
  slice(slice<U> other)
	: first(other.begin()),
	  last(other.end()) {

  }
};


// template<class G>
// using cslice = slice<const G>;


#endif
