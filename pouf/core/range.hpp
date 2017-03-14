#ifndef CORE_RANGE_HPP
#define CORE_RANGE_HPP

#include <utility>

// an iterator range
template<class Iterator>
struct range {
  Iterator first, last;

  Iterator begin() const { return first; }
  Iterator end() const { return last; }  
};

template<class Iterator>
static range<Iterator> make_range(Iterator first, Iterator last) {
  return {first, last};
}

template<class Iterator>
static range<Iterator> make_range(std::pair<Iterator, Iterator> pair) {
  return {pair.first, pair.second};
}


template<class C>
static auto reverse(C& container) -> decltype(make_range(container.rbegin(),
														 container.rend())) {
  return make_range(container.rbegin(),
					container.rend());
}




#endif
