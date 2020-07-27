#ifndef PARSER_HPP
#define PARSER_HPP

#include "either.hpp"
#include "unit.hpp"

#include <deque>
#include <functional>
#include <vector>

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <sstream>

#include <iostream>

namespace parser {

// character range
struct range {
  const char* first;
  const char* last;

  explicit operator bool() const { return first != last; }

  std::size_t size() const { return last - first; }

  char get() const {
    assert(bool(*this));
    return *first;
  }

  range next() const {
    assert(bool(*this));
    return {first + 1, last};
  }

};

  
static void peek(range self, std::ostream& out, std::size_t count=42) {
  for(std::size_t i = 0; self && (i < count);
            ++i, self = self.next()) {
    out << self.get();
  }
}
  

// successful parse  
template<class T>
struct success: range {
  using value_type = T;
  T value;
  success(T value, range rest): range(rest), value(std::move(value)) {}
};


template<class T>
static success<T> make_success(T value, range rest) {
  return {std::move(value), rest};
}

// parse error  
struct error: range {
  error(range at): range(at) {}
};


// parse result  
template<class T>
using result = either<error, success<T>>;


// parser value type  
template<class Parser>
using value =
    typename std::result_of<Parser(range)>::type::value_type::value_type;


// type-erased parser
template<class T>
using any = std::function<result<T>(range in)>;

////////////////////////////////////////////////////////////////////////////////
// combinators

// monad unit
template<class T>
static auto pure(T value) {
  return [value = std::move(value)](range in) -> result<T> {
    return make_success(value, in);
  };
};

// functor map
template<class Parser, class Func, class=value<Parser>>
static auto map(Parser parser, Func func) {
  return [parser = std::move(parser), func = std::move(func)](range in) {
    using value_type = typename std::result_of<Func(value<Parser>)>::type;
    using result_type = result<value_type>;
    return map(parser(in), [&](success<value<Parser>>&& self) {
      return make_success(func(std::move(self.value)), self);
    });
  };
};

template<class Parser, class Func, class=value<Parser>>
static auto operator|=(Parser parser, Func func) {
  return map(parser, func);
};


// monad bind
template<class Parser, class Func, class=value<Parser>>
static auto bind(Parser parser, Func func) {
  return [parser = std::move(parser), func = std::move(func)](range in) {
    using parser_type = typename std::result_of<Func(value<Parser>)>::type;
    using value_type = value<parser_type>;
    using result_type = result<value_type>;
    return parser(in) >>= [&](success<value<Parser>>&& self) -> result_type {
      const range in = self;
      if(auto res = func(std::move(self.value))(in)) {
        return res;
      }
      return error(in);
    };
  };
};

template<class Parser, class Func, class=value<Parser>>
static auto operator>>=(Parser parser, Func func) {
  return bind(parser, func);
};

// sequence parser  
template<class LHS, class RHS, class=value<LHS>, class=value<RHS>>
static auto operator>>(LHS lhs, RHS rhs) {
  return lhs >>= [rhs = std::move(rhs)](auto) { return rhs; };
};

// guarded continuation  
template<class Pred>
static auto guard(Pred pred) {
  return [pred = std::move(pred)](auto value) {
    using value_type = decltype(value);

    return [pred, value = std::move(value)](range in) -> result<value_type> {
      if(pred(value)) {
        return make_success(value, in);
      }
      return error(in);
    };
  };
};

// drop continuation  
template<class Parser>
static auto drop(Parser parser) {
  return [parser = std::move(parser)](auto value) {
    return parser >> pure(std::move(value));
  };
}

// reference parser  
template<class Parser>
static auto ref(const Parser& parser) {
  return [&](range in) -> result<value<Parser>> { return parser(in); };
}

// kleen star parser (zero-or-more). note: always succeeds  
template<class Parser>
static auto kleene(Parser parser) {
  return [parser = std::move(parser)](range in) -> result<std::deque<value<Parser>>> {
    std::deque<value<Parser>> values;
    while(auto result = parser(in)) {
      values.emplace_back(std::move(result.get()->value));
      in = *result.get();
    }

    return make_success(std::move(values), in);
  };
};

// one-or-more parser
template<class Parser>
static auto plus(Parser parser) {
  return parser >>= [parser](auto first) {
    return kleene(parser) >>= [first](auto rest) {
      rest.emplace_front(std::move(first));
      return pure(rest);
    };
  };
};


// coproduct parser (alternative). note: rhs is only called if lhs fails
template<class LHS, class RHS>
static auto coproduct(LHS lhs, RHS rhs) {
  return [lhs = std::move(lhs), rhs = std::move(rhs)](range in) {
    if(auto res = lhs(in)) {
      return res;
    } else
      return rhs(in);
  };
};


template<class LHS, class RHS, class=value<LHS>, class=value<RHS>>
static auto operator|(LHS lhs, RHS rhs) {
  return coproduct(std::move(lhs), std::move(rhs));
}

// non-empty list with separator  
template<class Parser, class Separator>
static auto list(Parser parser, Separator separator) {
  return parser >>= [=](auto first) {
    return kleene(separator >> parser) >>=
           [first = std::move(first)](auto rest) {
             rest.emplace_front(first);
             return pure(rest);
           };
  };
};


template<class Parser, class Separator, class=value<Parser>, class=value<Separator>>
static auto operator%(Parser parser, Separator separator) {
  return list(std::move(parser), std::move(separator));
}


// skip parser: parse zero-or-more without collecting  
template<class Parser>
static auto skip(Parser parser) {
  return [parser = std::move(parser)](range in) -> result<unit> {
    while(auto res = parser(in)) {
      in = *res.get();
    }
    return make_success(unit{}, in);
  };
}

// tokenizer: skip before parsing
template<class Parser, class Skipper>
static auto token(Parser parser, Skipper skipper) {
  return skip(skipper) >> parser;
}

// debugparser
template<class Parser>
static auto debug(std::string name, Parser parser,
                  std::ostream& out = std::clog) {
  return [name = std::move(name), parser = std::move(parser), &out](range in) {
    out << "> " << name << ": \"";
    peek(in, out);
    out << "\"\n";
    
    auto res = parser(in);
    if(res) {
      out << "< " << name << " ok: \"";
      peek(*res.get(), out);
      out << "\"\n";
    } else {
      std::clog << "< " << name << " failed\n";
    }

    return res;
  };
}

// longest parse: runs both parsers and keep the longest match. note: lhs result
// is returned in case of a draw.
template<class LHS, class RHS>
static auto longest(LHS lhs, RHS rhs) {
  return [lhs = std::move(lhs), rhs = std::move(rhs)](range in) {
    auto as_lhs = lhs(in);
    auto as_rhs = rhs(in);

    if(!as_rhs)
      return as_lhs;
    if(!as_lhs)
      return as_rhs;

    if(as_lhs.get()->first >= as_rhs.get()->first) {
      return as_lhs;
    } else {
      return as_rhs;
    }
  };
};

// fixpoint (result type needs to be given because c++)
template<class T, class Def>
struct fixpoint {
  const Def def;
  
  result<T> operator()(parser::range in) const {
    return def(*this)(in);
  }
};

template<class T, class Def>
static fixpoint<T, Def> fix(Def def) {
  return {def};
}
  

////////////////////////////////////////////////////////////////////////////////
// concrete parsers

// char parser  
static result<char> _char(range in) {
  if(!in) {
    return error(in);
  }
  
  return make_success(in.get(), in.next());
};

// parse char matching a predicate  
template<class Pred>
static auto pred(Pred pred) {
  return _char >>= guard(pred);
}

static auto pred(int (*pred)(int)) { return _char >>= guard(pred); }
  
// parse a given char  
static auto single(char c) {
  return pred([c](char x) { return x == c; });
}

// parse a fixed keyword 
static auto keyword(std::string value) {
  return [value = std::move(value)](range in) -> result<unit> {
    if(in.size() < value.size()) {
      return error(in);
    }

    if(value.compare(0, value.size(), in.first, value.size()) == 0) {
      return make_success(unit{}, range{in.first + value.size(), in.last});
    }

    return error(in);
  };
}

// parse numbers. note: leading whitespaces (as per std::isspace) are consumed
static result<double> _double(range in) {
  if(!in)
    return error(in);
  char* end;
  const double res = std::strtod(in.first, &end);
  if(end == in.first)
    return error(in);
  return make_success(res, range{end, in.last});
}

static result<float> _float(range in) {
  if(!in)
    return error(in);
  char* end;
  const float res = std::strtof(in.first, &end);
  if(end == in.first)
    return error(in);
  return make_success(res, range{end, in.last});
}

static result<long> _long(range in) {
  if(!in)
    return error(in);
  char* end;
  const long res = std::strtol(in.first, &end, 10);
  if(end == in.first)
    return error(in);
  return make_success(res, range{end, in.last});
}

static result<unsigned long> _unsigned_long(range in) {
  if(!in)
    return error(in);
  char* end;
  const unsigned long res = std::strtoul(in.first, &end, 10);
  if(end == in.first)
    return error(in);
  return make_success(res, range{end, in.last});
}

// convenience tokenizer  
template<class Parser>
static auto token(Parser parser) {
  return skip(pred(std::isspace)) >> parser;
}

// end of stream parser  
static result<unit> eos(range in) {
  if(in.first == in.last) {
    return make_success(unit{}, in);
  }

  return error(in);
}

////////////////////////////////////////////////////////////////////////////////
template<class Parser>
static value<Parser> run(Parser parser, range in) {
  // TODO report line/col
  return match(
      parser(in),
      [=](error err) -> value<Parser> {
        std::stringstream ss;
        ss << "parse error near \"";
        peek(err, ss);
        ss << "\"";
        throw std::runtime_error(ss.str());
      },
      [](success<value<Parser>>& ok) { return std::move(ok.value); });
}

template<class Parser>
static value<Parser> run(Parser parser, const char* in) {
  return run(parser, range{in, in + std::strlen(in)});
}


template<class Parser>
static auto run(Parser parser, std::istream& in) {
  const std::string contents(std::istreambuf_iterator<char>(in), {});
  return run(parser, range{contents.data(), contents.data() + contents.size()});
}

} // namespace parser

#endif
