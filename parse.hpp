#ifndef PARSE_HPP
#define PARSE_HPP

#include <istream>
#include <vector>
#include <cassert>

#include <iostream>
#include <sstream>
#include <functional>

namespace parse {

  // recover type U for F: T -> M<U> 
  template<template<class> class M, class F, class T, class = typename std::result_of<F(T)>::type>
  struct monad_bind_type;
  
  template<template<class> class M, class F, class T, class U>
  struct monad_bind_type<M, F, T, M<U> > {
    using value_type = U;
  };

  
  // maybe monad
  template<class T> class maybe;
  

  template<class T>
  class maybe {
    using storage_type = typename std::aligned_union<0, T>::type;
    storage_type storage;
    const bool flag;
  public:
    using value_type = T;
    
    maybe() : flag(false) { }
    
    maybe(const value_type& value) : flag(true) {
      new (&storage) value_type(value);
    }

    maybe(value_type&& value) : flag(true) {
      new (&storage) value_type(std::move(value));
    }

    ~maybe() { 
      if(flag) get().~value_type();
    }


    maybe(const maybe& other) : flag(other.flag) {
      if(flag) new (&storage) value_type(other.get());
    }

    maybe(maybe&& other) : flag(other.flag) {
      if(flag) new (&storage) value_type(std::move(other.get()));
    }

    maybe& operator=(const maybe& ) = delete;
    maybe& operator=(maybe&& ) = delete;    
    
    explicit operator bool() const { return flag; }

    const value_type& get() const { return reinterpret_cast<const value_type&>(storage); }
    value_type& get() { return reinterpret_cast<value_type&>(storage); }


    // monadic bind
    template<class F>
    using bind_type = maybe< typename monad_bind_type< parse::maybe, F, value_type >::value_type >;
    
    template<class F>
    bind_type<F> operator>>(const F& f) const & {
      if(!flag) return {};
      return f(get());
    }

    template<class F>
    bind_type<F> operator>>(const F& f) && {
      if(!flag) return {};
      return f( std::move(get()) );
    }
    
  };


  // save/restore stream position
  struct stream_pos {
    std::istream& in;
    const std::istream::pos_type pos;
    stream_pos(std::istream& in) : in(in), pos(in.tellg()) { }

    void reset() {
      in.clear();
      in.seekg(pos);
    }
    
  };

  
  // parser value type
  template<class Parser>
  using value_type = typename monad_bind_type<maybe, Parser, std::istream& >::value_type;

  
  template<class LHS, class RHS>
  struct coproduct_type {
    
    const LHS lhs;
    const RHS rhs;

    static_assert( std::is_same< value_type<LHS>, value_type<RHS> >::value, 
                   "parsers should have the same type");

    using T = parse::value_type<LHS>;
  
    maybe<T> operator()(std::istream& in) const {

      if(const maybe<T> tmp = lhs(in)) {
        return tmp;
      }

      return rhs(in);
    }
  
  };


  template<class LHS, class RHS>
  static inline coproduct_type<LHS, RHS> operator | (LHS lhs, RHS rhs) {
    return {lhs, rhs};
  }


  // monadic return
  template<class T>
  struct pure_type {
    const T value;
  
    maybe<T> operator()(std::istream& in) const {
      return value;
    }
  
  };

  template<class T>
  static inline pure_type<T> pure(T value) { return {value}; }
  
  
  // monadic zero
  template<class T>
  struct fail_type {

    maybe<T> operator()(std::istream& in) const {
      return {};
    }
    
  };


  // monadic bind
  template<class Parser, class F>
  struct bind_type {

    const Parser parser;
    const F f;

    using T = parse::value_type<Parser>;
    using result_type = typename std::result_of< F(T) >::type;
    using U = parse::value_type<result_type>;
    
    maybe<U> operator()(std::istream& in) const {
      // backup stream position
      stream_pos pos(in);
      
      const maybe<U> res = parser(in) >> [&](T&& source) {
        return f( std::move(source) )(in);
      };

      // rollback stream if needed
      if(!res) pos.reset();
      
      return res;
    }
  
  };


  template<class Parser, class F, class Check = value_type<Parser> >
  static inline bind_type<Parser, F> operator>>(Parser parser, F f) {
    return {parser, f};
  }

  // sequence without binding: a >> then(b) 
  template<class Parser>
  struct then_type {
    const Parser parser;

    template<class T>
    Parser operator()(T&&) const {
      return parser;
    }
  };


  template<class Parser>
  static then_type<Parser> then(Parser parser) {
    return {parser};
  }


  // convenience
  template<class LHS, class RHS>
  static bind_type<LHS, then_type<RHS> > operator,(LHS lhs, RHS rhs) {
    return lhs >> then(rhs);
  }
  

  // use parser then ignore result and return previously bound value
  template<class Parser>
  struct drop_type {
    const Parser parser;
    
    template<class T>
    bind_type<Parser, then_type<pure_type< typename std::decay<T>::type > >> operator()(T&& value) const {
      return parser >> then( pure( std::forward<T>(value) ) );
    };
    
  };
  

  template<class Parser>
  static drop_type<Parser> drop(Parser parser) {
    return {parser};
  }
  
  
  // kleene star
  template<class Parser>
  struct kleene_type {
    const Parser parser;

    using T = parse::value_type<Parser>;
    using U = std::vector<T>;

    maybe<U> operator()(std::istream& in) const {

      U res;
      while( maybe<T> item = parser(in) ) {
        res.emplace_back( std::move(item.get()) );
      }
      
      return res;
    }
  };
  

  template<class Parser>
  static inline kleene_type<Parser> operator*(Parser parser) { return {parser}; }


  template<class Parser>
  struct repeat_type {
    const Parser parser;
    const std::size_t n;

    using T = parse::value_type<Parser>;
    using U = std::vector<T>;
    
    maybe<U> operator()(std::istream& in) const {
      // backup stream position
      stream_pos pos(in);

      U res; res.reserve(n);
      
      for(std::size_t i = 0; i < n; ++i) {
        const maybe<T> value = parser(in);
        
        if(value) {
          res.emplace_back( std::move(value.get()) );
        } else {
          pos.reset();
          return {};
        }
      }
      
      return res;
    }
  };


  template<class Parser>
  static repeat_type<Parser> operator*(Parser parser, std::size_t n) {
    return {parser, n};
  }
  

  // literal parser
  template<class T>
  struct literal_type {

    maybe<T> operator()(std::istream& in) const {
      T res;

      if(in >> res) return res;
      return {};
    }
    
  };



  // character parser from predicate
  template<class Pred>
  struct chr_type {
    const Pred pred;
  
    maybe<char> operator()(std::istream& in) const {
      const int c = in.get();

      if(c == std::istream::traits_type::eof()) {
        return {};
      }
      
      if( pred(c) ) {
        return c;
      }

      in.putback(c);
      return {};
    }

    struct negate {
      const Pred pred;
      bool operator()(char c) const { return !pred(c); }
    };

    
    chr_type< negate > operator!() const {
      return {{pred}};
    }
    
  };


  template<char c> static inline bool equals(char x) { return x == c; }

  template<class Pred>
  static inline chr_type<Pred> chr(Pred pred) { return {pred}; }

  template<char c>
  static inline chr_type< bool (*)(char) > chr() { return chr(equals<c>); }

  struct in_set {
    const char* source;
    
    bool operator()(const char& x) const {
      for(const char* ptr = source; *ptr; ++ptr) {
        if(x == *ptr) return true;
      }
      return false;
    }
  };
  

  inline chr_type<in_set> chr(const char* set) {
    return {{set}};
  }

  // convenience for std::isspace and friends
  static inline chr_type<int (*)(int)> chr( int(*pred)(int) ) {
    return {pred};
  }
  
  
  // string literals. pred tells separators.
  template<class Pred>
  struct str_type {
    const std::string value;
    const Pred pred;
    
    str_type(const std::string& value, const Pred pred)
      : value(value),
        pred(pred){ }
    
    maybe<const char*> operator()(std::istream& in) const {
      // backup stream position
      stream_pos pos(in);

      for(const char c : value) {

        if(!chr( [c](char x) { return x == c; })(in)) {
          pos.reset();
          return {};
        } else {

        }
      }

      const int next = in.peek();
      
      if(in.eof() || pred(next) ) {
        return {value.c_str()};
      } else {
        pos.reset();
        return {};
      }
    }
    
  };


  template<class Pred = int (*)(int)>
  static str_type<Pred> str(const std::string& value, Pred pred = std::isspace) {
    return {value, pred};
  }
  
  
  template<class T> using lit = literal_type<T>;
  template<class T> using fail = fail_type<T>;  


  // convenience
  static std::size_t debug_indent = 0;
  
  template<class Parser>
  struct debug_type {
    const Parser parser;
    const char* name;
    std::ostream& out;
    
    static std::ostream& indent(std::ostream& out, std::size_t depth) {
      for(auto i = 0; i < depth; ++i) out << "  ";
      return out;
    }
    
    using T = parse::value_type<Parser>;
    
    maybe<T> operator()(std::istream& in) const {
      indent(out, debug_indent++) << ">> " << name << std::endl;
      const maybe<T> res = parser(in);
      indent(out, --debug_indent) << "<< " << name << ": " << (res ? "success" : "failed") << std::endl;;
      return res;
    }
    
  };

  struct debug {

    // TODO #ifdef
    static const bool enabled = false;
    
    const char* name;
    std::ostream& out;

    debug(const char* name, std::ostream& out = std::clog)
      : name(name),
        out(out) {

    }

    
    template<class Parser>
    typename std::enable_if<debug::enabled, debug_type<Parser>>::type operator<<=(Parser parser) const {
      return {parser, name, out};
    }

    template<class Parser>
    typename std::enable_if<!debug::enabled, Parser>::type operator<<=(Parser parser) const {
      return parser;
    }
    
  };
  

  template<class Parser, class Skip>
  struct token_type {
    const Parser parser;
    const Skip skip;

    using T = parse::value_type<Parser>;

    maybe<T> operator()(std::istream& in) const {
      // a token just skips leading spaces
      while( skip(in) );
      return parser(in);      
    }
    
  };

  

  template<class Parser, class Skip = chr_type<int(*)(int)> >
  static token_type<Parser, Skip> token(Parser parser, Skip skip = {std::isspace} ) {
    return {parser, skip};
  }


  // type erasure
  template<class T>
  using any = std::function< maybe<T> (std::istream& ) >;


  // non-empty sequences
  template<class Parser>
  struct plus_type {
    const Parser parser;
    
    using T = parse::value_type<Parser>;
    using U = std::vector<T>;
    
    maybe<U> operator()(std::istream& in) const {

      return (*parser)(in) >> [](U&& ts) -> maybe<U> {
        if(ts.empty()) return {};
        return std::move(ts);
      };
      
    }
    
  };

  template<class Parser>
  static inline plus_type<Parser> operator+(Parser parser) { return {parser}; }


  
  // sequence parser
  template<class Parser, class Separator>
  static any< std::vector< parse::value_type<Parser> > > sequence(Parser parser, Separator separator) {
    using value_type = parse::value_type<Parser>;

    using vec = std::vector<value_type>;

    static const auto empty = pure( vec() );
    
    const auto non_empty =
      *(parser >> drop(separator)) >> [parser](vec&& xs) {
      return parser >> [&xs](value_type&& last) {
        xs.emplace_back(last);
        return pure( std::move(xs) );
      };
    };

    return non_empty | empty;
  }


  // fixed-length sequence parser
  template<class Parser, class Separator>
  static any< std::vector< parse::value_type<Parser> > > sequence(Parser parser, 
                                                                  Separator separator, 
                                                                  std::size_t size) {
    using value_type = parse::value_type<Parser>;
    using vec = std::vector<value_type>;

    if(size == 0) {
      throw std::runtime_error("cannot parse fixed-length empty sequences");
    }
    
    return (parser >> drop(separator)) * (size - 1) >> [parser](vec&& xs) {
      return parser >> [&xs](value_type&& x) {
        xs.emplace_back(x);
        return pure(std::move(xs));
      };
    };
    
  }

  struct eof {

    maybe<eof> operator()(std::istream& in) const {
      in.peek();
      if(in.eof()) return eof();
      return {};
    }
    
  };


  // recursive rules (need unique tag, assign only once)

  template<class Type, class Tag>
  struct rec {
    using impl_type = maybe<Type> (*)(std::istream&);
    static impl_type impl;

    // TODO use stateful friend injection magic to detect at compile-time?
    maybe<Type> operator()(std::istream& in) const {
      if(!impl) throw std::runtime_error("parser not defined!");
      return impl(in);
    }


    template<class Parser>
    rec& operator=(Parser parser) {
      static const Parser copy = parser;
      
      static_assert( std::is_same< value_type<Parser>, Type >::value,
                     "parser type error" );
      
      impl = [](std::istream& in) -> maybe<Type> {
        return copy(in);
      };

      return *this;
    }

  };


  template<class Type, class Tag>
  typename rec<Type, Tag>::impl_type rec<Type, Tag>::impl;
  
}


#endif
