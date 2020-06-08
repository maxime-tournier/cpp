#ifndef PARSER_HPP
#define PARSER_HPP

#include "either.hpp"

#include <cassert>
#include <sstream>

namespace parser {

  struct range {
    const char* first;
    const char* last;
    
    explicit operator bool() const {
      return first != last;
    }

    char get() const {
      assert(bool(*this));      
      return *first;
    }
    
    range next() const {
      assert(bool(*this));
      return {first + 1, last};
    }
    
  };

  template<class T>
  struct success {
    using value_type = T;
    const T value;
    const range rest;
  };

  template<class T>
  using result = either<range, success<T>>;

  template<int (*pred) (int)>
  static result<char> chr(range in) {
    if(!in) return in;
    if(!pred(in.get())) return in;
    return success<char>{in.get(), in.next()};
  };

  template<class Parser>
  using value_type = typename std::result_of<Parser(range)>::type::value_type::value_type;

  template<class Parser>
  static value_type<Parser> run(Parser parser, range in,
                                std::size_t size=24) {
    return match(parser(in),
                 [=](range where) -> value_type<Parser> {
                   std::stringstream ss;
                   ss << "parse error near \"";
                   for(std::size_t i = 0; where && (i < size);
                       ++i, where = where.next()) {
                     ss << where.get();
                   }
                   ss << "\"";
                   throw std::runtime_error(ss.str());
                 },
                 [](const success<value_type<Parser>>& ok) {
                   return ok.value;
                 });
  }
  
}

#endif

