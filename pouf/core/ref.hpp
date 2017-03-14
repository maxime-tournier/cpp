#ifndef REF_HPP
#define REF_HPP

#include <memory>

template<class T>
using ref = std::shared_ptr<T>;

#endif
