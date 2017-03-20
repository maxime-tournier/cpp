#ifndef CORE_SLICE_HPP
#define CORE_SLICE_HPP

#include <cassert>

// an array slice
template<class G>
class slice {
protected:
  G* first;
  G* last;

  slice() {  }
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



  slice& operator<<(const slice<const G>& other) {
    assert(size() == other.size());
    std::copy(other.begin(), other.end(), begin());
    return *this;
  }
  
  template<class U>
  slice(slice<U> other)
	: first(other.begin()),
	  last(other.end()) {

  }

  template<class U>
  slice<const U> cast() const {
    return {reinterpret_cast<const U*>(begin()),
        reinterpret_cast<const U*>(end())};
        
  }

  template<class U>
  slice<U> cast() {
    return {reinterpret_cast<U*>(begin()),
        reinterpret_cast<U*>(end())};
  }

  
};


// template<class G>
// using cslice = slice<const G>;


#endif
