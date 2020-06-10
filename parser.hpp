#ifndef PARSER_HPP
#define PARSER_HPP

#include "either.hpp"

#include <vector>
#include <deque>
#include <functional>

#include <cassert>
#include <cstdlib>
#include <sstream>

#include <iostream>

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
  struct success: range {
    using value_type = T;
    T value;
    success(T value, range rest): range(rest), value(std::move(value)) { }
  };

  struct error: range { using range::range; };
    
  
  template<class T>
  static success<T> make_success(T value, range rest) {
    return {std::move(value), rest};
  }
  
  template<class T>
  using result = either<range, success<T>>;

  template<class Parser>
  using value =
      typename std::result_of<Parser(range)>::type::value_type::value_type;


  template<class T>
  using any = std::function<result<T>(range in)>;
  
  ////////////////////////////////////////////////////////////////////////////////
  // combinators
  
  template<class T>
  static auto unit(T value) {
    return [value=std::move(value)](range in) -> result<T> {
      return make_success(value, in);
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
            [&](const auto& ok) -> result_type {
              return make_success(func(ok.value), ok.rest);
            });
    };
  };


  template<class Parser, class Func>
  static auto bind(Parser parser, Func func) {
    return [parser=std::move(parser),
            func=std::move(func)](range in) {
      using parser_type = typename std::result_of<Func(value<Parser>)>::type;
      using value_type = value<parser_type>;
      using result_type = result<value_type>;
      return match(parser(in),
            [](range fail) -> result_type { return fail; },
            [&](const auto& ok) -> result_type {
              // TODO move into func
              return func(ok.value)(ok.rest);
            });
    };
  };

  template<class Parser, class Func>
  static auto operator>>=(Parser parser, Func func) {
    return bind(parser, func);
  };

  template<class LHS, class RHS>
  static auto operator>>(LHS lhs, RHS rhs) {
    return lhs >>= [rhs = std::move(rhs)](auto) { return rhs; };
  };


  template<class Parser>
  static auto drop(Parser parser) {
    return [parser=std::move(parser)](auto value) {
      return parser >> unit(std::move(value));
    };
  }

  template<class Parser>
  static auto ref(const Parser& parser) {
    return [&](range in) { return parser(in); };
  }
  
  template<class Parser>
  static auto kleene(Parser parser) {
    return [parser = std::move(parser)](range in)
      -> result<std::deque<value<Parser>>> {
      std::deque<value<Parser>> values;
      while(auto result = parser(in).get()) {
        values.emplace_back(std::move(result->value));
        in = result->rest;
      }

      return make_success(std::move(values), in);
    };
  };


  template<class Parser>
  static auto plus(Parser parser) {
    return parser >>= [parser](auto first) {
      return kleene(parser) >>= [first](auto rest) {
        rest.emplace_front(std::move(first));
        return unit(rest);
      };
    };
  };
  

  template<class LHS, class RHS>
  static auto coproduct(LHS lhs, RHS rhs) {
    return [lhs = std::move(lhs),
            rhs = std::move(rhs)](range in) {
      if(auto res = lhs(in)) {
        return res;
      } else return rhs(in);
    };
  };


  template<class LHS, class RHS>
  static auto operator|(LHS lhs, RHS rhs) {
    return coproduct(std::move(lhs), std::move(rhs));
  }

  template<class Parser, class Separator>
  static auto list(Parser parser, Separator separator) {
    return parser >>= [=](auto first) {
      return kleene(separator >> parser) >>= [first=std::move(first)](auto rest) {
        rest.emplace_front(first);
        return unit(rest);
      };
    };
  };


  template<class Parser, class Separator>
  static auto operator%(Parser parser, Separator separator) {
    return list(std::move(parser), std::move(separator));
  }


  template<class Parser>
  static auto skip(Parser parser) {
    return [parser=std::move(parser)](range in) -> result<bool> {
      while(auto res = parser(in)) {
        in = res.get()->rest;
      }
      return make_success(true, in);
    };
  }
  

  template<class Parser, class Skipper>
  static auto token(Parser parser, Skipper skipper) {
    return skip(skipper) >> parser;
  }

  template<class Parser>
  static auto debug(std::string name, Parser parser) {
    return [name=std::move(name),
            parser=std::move(parser)](range in) {
      std::clog << "> " << name << ": \"" << in.first << "\"\n" ;
      auto res = parser(in);
      if(res) {
        std::clog << "< " << name << " ok: \"" << res.get()->rest.first << "\"\n" ;     
      } else {
        std::clog << "< " << name << " failed\n";
      }

      return res;
    };
  }


  template<class LHS, class RHS>
  static auto longest(LHS lhs, RHS rhs) {
    return [lhs=std::move(lhs),
            rhs=std::move(rhs)](range in) {
      auto as_lhs = lhs(in);
      auto as_rhs = rhs(in);

      if(!as_rhs) return as_lhs;
      if(!as_lhs) return as_rhs;

      if(as_lhs.get()->rest.first >= as_rhs.get()->rest.first) {
        return as_lhs;
      } else {
        return as_rhs;
      }
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
  static result<char> single(range in) {
    return _char<equals<c>>(in);
  }

  // numbers
  static result<double> _double(range in) {
    if(!in) return in;
    char* end;
    const double res = std::strtod(in.first, &end);
    if(end == in.first) return in;
    return make_success(res, range{end, in.last});
  }

  static result<float> _float(range in) {
    if(!in) return in;
    char* end;
    const float res = std::strtof(in.first, &end);
    if(end == in.first) return in;
    return make_success(res, range{end, in.last});
  }
  
  static result<long> _long(range in) {
    if(!in) return in;
    char* end;
    const long res = std::strtol(in.first, &end, 10);
    if(end == in.first) return in;
    return make_success(res, range{end, in.last});
  }

  static result<unsigned long> _unsigned_long(range in) {
    if(!in) return in;
    char* end;
    const unsigned long res = std::strtoul(in.first, &end, 10);
    if(end == in.first) return in;
    return make_success(res, range{end, in.last});
  }

  
  template<class Parser>
  static auto token(Parser parser) {
    return skip(_char<std::isspace>) >> parser;
  }

  static result<bool> eos(range in) {
    if(in.first == in.last) {
      return make_success(true, in);
    }

    return in;
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

