#ifndef SLIP_COMMON_HPP
#define SLIP_COMMON_HPP

#include <string>
#include <sstream>

template<class T>
static std::string quote(const T& self) {
  std::stringstream ss;
  ss << '"' << self << '"';
  return ss.str();
}


template<class T>
static std::string parens(const T& self) {
  std::stringstream ss;
  ss << '(' << self << ')';
  return ss.str();
}




#endif
