#ifndef PARSER_HPP
#define PARSER_HPP

#include "either.hpp"
#include <vector>

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
    T value;
    range rest;
  };

  template<class T>
  static success<T> make_success(T value, range rest) {
    return {std::move(value), rest};
  }
  
  template<class T>
  using result = either<range, success<T>>;

  template<class Parser>
  using value =
      typename std::result_of<Parser(range)>::type::value_type::value_type;


  ////////////////////////////////////////////////////////////////////////////////
  // combinators

  template<class T>
  static auto pure(T value) {
    return [value=std::move(value)](range in) -> result<T> {
      return value;
    };
  };

  template<class Parser, class Func>
  static auto map(Parser parser, Func func) {
    return [parser=std::move(parser),
            func=std::move(func)](range in) {
      using value_type = typename std::result_of<Func(value<Parser>)>::type;
      using result_type = result<value_type>;
      return match(parser(in),
            [](range fail) -> result_type { return fail; },
            [](const auto& ok) -> result_type {
              return make_success(f(ok.value), ok.rest);
            });
    };
  };


  
  template<class Parser>
  static auto kleene(Parser parser) {
    return [parser = std::move(parser)](range in)
      -> result<std::vector<value<Parser>>> {
      std::vector<value<Parser>> values;
      while(auto result = parser(in).get()) {
        values.emplace_back(std::move(result->value));
        in = result->rest;
      }

      return make_success(std::move(values), in);
    };
  };
  

  ////////////////////////////////////////////////////////////////////////////////
  // concrete parsers
  
  template<int (*pred) (int)>
  static result<char> _char(range in) {
    if(!in) return in;
    if(!pred(in.get())) return in;
    return success<char>{in.get(), in.next()};
  };

  template<char c>
  static int equals(int x) { return x == c; }

  template<char c>
  static int any(int) { return true; }
  
  template<char c>
  static result<char> single(range in) {
    return _char<equals<c>>(in);
  }

  
  
  ////////////////////////////////////////////////////////////////////////////////
  static constexpr std::size_t error_length = 24;
  
  template<class Parser>
  static value<Parser> run(Parser parser, range in) {
    // TODO report line/col
    return match(parser(in),
                 [=](range where) -> value<Parser> {
                   std::stringstream ss;
                   ss << "parse error near \"";
                   for(std::size_t i = 0; where && (i < error_length);
                       ++i, where = where.next()) {
                     ss << where.get();
                   }
                   ss << "\"";
                   throw std::runtime_error(ss.str());
                 },
                 [](const success<value<Parser>>& ok) {
                   return ok.value;
                 });
  }

  template<class Parser>
  static auto run(Parser parser, std::istream& in) {
    const std::string contents(std::istreambuf_iterator<char>(in), {});
    return run(parser,
               range{contents.data(), contents.data() + contents.size()});
  }
  
}

#endif

