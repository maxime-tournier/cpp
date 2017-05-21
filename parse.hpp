#ifndef PARSE_HPP
#define PARSE_HPP

#include <istream>
#include <deque>

#include "maybe.hpp"

// monadic parser generators

namespace parse {

  // parser traits
  template<class Parser>
  struct traits {
    using value_type = typename Parser::value_type;
  };


  template<class T>
  struct traits< maybe<T> (*)(std::istream& ) > {
    using value_type = T;
  };

  
  // monadic return
  template<class T>
  struct pure_type {
    using value_type = T;

    const value_type value;
  
    maybe<value_type> operator()(std::istream& in) const {
      return value;
    }
  
  };
  
  template<class T>
  static pure_type<T> pure(const T& value) { return {value}; }


  // lazy result
  template<class F>
  struct lazy_type {
    using value_type = typename std::result_of< F(void) >::type;

    const F f;
  
    maybe<value_type> operator()(std::istream& in) const {
      return f();
    }
  
  };

  template<class F>
  static lazy_type<F> lazy(const F& f) { return {f}; }


  // monadic zero
  template<class T>
  struct fail_type {
    using value_type = T;

    maybe<value_type> operator()(std::istream& in) const {
      return none();
    }
  
  };


  // sequencing with binding
  template<class Parser, class F>
  struct bind_type {
    const Parser parser;
    const F f;

    using domain_type = typename traits<Parser>::value_type;
    
    using range_type = typename std::result_of< F(domain_type) >::type;
    using value_type = typename range_type::value_type;

    maybe<value_type> operator()(std::istream& in) const {
      const auto pos = in.tellg();
      
      maybe<domain_type> value = parser(in);

      if(!value) return none();

      const maybe<value_type> res = f( std::move(value.template get<domain_type>() ) )(in);
      if(res) return res;

      // backtrack
      in.seekg(pos);
      in.clear();      

      return none();
    }
  
  };


  template<class Parser, class F>
  static bind_type<Parser, F> bind(const Parser& parser, const F& f) {
    return {parser, f};
  }


  template<class Parser, class F>
  static bind_type<Parser, F> operator>>(const Parser& parser, const F& f) {
    return {parser, f};
  }


  // sequencing without binding (last parser result is returned)
  template<class Parser>
  struct then_type {
    const Parser parser;
    
    template<class U>
    Parser operator()(const U& ) const {
      return parser;
    }
    
  };

  template<class Parser>
  static then_type<Parser> then(Parser parser) {
    return {parser};
  }

  template<class LHS, class RHS>
  static bind_type<LHS, then_type<RHS> > operator,(LHS lhs, RHS rhs) {
    return lhs >> then(rhs);
  }

  
  
  // alternatives (short-circuits as expected)
  template<class LHS, class RHS>
  struct alt_type {
    const LHS lhs;
    const RHS rhs;

    static_assert( std::is_same< typename LHS::value_type,
                   typename RHS::value_type>::value,
                   "types must be the same" );
    
    using value_type = typename LHS::value_type;
    
    maybe<value_type> operator()(std::istream& in) const {
      const maybe<value_type> left = lhs(in);
      if(left) return left;
      return rhs(in);
    }
  
  };

  template<class LHS, class RHS>
  static alt_type<LHS, RHS> alt(LHS lhs, RHS rhs) {
    return {lhs, rhs};
  }


  template<class LHS, class RHS>
  static alt_type<LHS, RHS> operator|(LHS lhs, RHS rhs) {
    return {lhs, rhs};
  }


  // kleene star: matches any (possibly zero) consecutive occurences
  template<class Parser>
  struct kleene_type {
    const Parser parser;

    using source_type = typename traits<Parser>::value_type;
    using value_type = std::deque< source_type >;

    maybe<value_type> operator()(std::istream& in) const {
      value_type res;
      
      while( maybe<source_type> x = parser(in) ) {
        res.push_back(x.template get<source_type>() );
      }

      return res;
    }

  };


  template<class Parser>
  static kleene_type<Parser> kleene(const Parser& parser) {
    return {parser};
  }

  template<class Parser>
  static kleene_type<Parser> operator*(const Parser& parser) {
    return kleene_type<Parser>{parser};
  }



  // character matching predicate
  template<class Pred>
  struct character {
	const Pred pred;

	struct negate {
	  const Pred pred;

	  bool operator()(char c) const { return !pred(c); }
	};

	character<negate> operator!() const { return {pred}; }
    
    using value_type = char;
    
    maybe<value_type> operator()(std::istream& in) const {
      char c;

      if(in >> c) {
        if( pred(c) ) return c;
        in.putback(c);
      }
      
      return none();
    }

  }; 
  
  struct equals {
	const char expected;

	bool operator()(char c) const { return c == expected; }
  };
  
  static character<equals> chr(char c) { return {{c}}; }

  template<int (*pred)(int) > struct adaptor {
    bool operator()(char c) const { return pred(c); }
  };

  template<int (*pred)(int)> static character<adaptor<pred>> chr() { return {}; }



  // literal value
  template<class T>
  struct lit {

    using value_type = T;
    
    maybe<value_type> operator()(std::istream& in) const {
      const auto pos = in.tellg();
      
      value_type value;
      if( in >> value ) {
        return value;
      }
      
      in.seekg(pos);
      in.clear();

      return none();
    }
   
  };


  // disable whitespace skipping
  template<class Parser>
  struct no_skip_type {
    const Parser parser;

    using value_type = typename traits<Parser>::value_type;

    maybe<value_type> operator()(std::istream& in) const {
      if(in.flags() & std::ios::skipws) {
        in >> std::noskipws;

        const maybe<value_type> res = parser(in);

        in >> std::skipws;
        return res;
      } else {
        return parser(in);
      }
    }
      
  };


  template<class Parser>
  static no_skip_type<Parser> no_skip(const Parser& parser) { return {parser}; }


  // type erasure
  template<class T>
  struct any : std::function< maybe<T>(std::istream& in) >{
    using value_type = T;
    
    using any::function::function;
  };


  // recursive parsers: Tag should be a unique type for each parser
  template<class T, class Tag>
  struct rec {
    using value_type = T;
    using impl_type = maybe<T> (*)(std::istream& in);
    
    static impl_type impl;

    maybe<T> operator()(std::istream& in) const {
      if(!impl) throw std::runtime_error("empty recursive parser");
      return impl(in);
    }

    template<class F>
    rec& operator=(const F& other) {
      static const F closure = other;
      impl = [](std::istream& in) -> maybe<T> {
        return closure(in);
      };

      return *this;
    };
    
  };

  template<class T, class Tag>
  typename rec<T, Tag>::impl_type rec<T, Tag>::impl = nullptr;
  
  
  // cast value (use with bind)
  template<class T>
  struct cast {
    
    template<class U>
    pure_type<T> operator()(const U& u) const {
      return pure<T>(u);
    } 
    
  };
  

  // non-empty sequence
  template<class Parser>
  struct plus_type {
    const Parser parser;

    using source_type = typename traits<Parser>::value_type;
    using value_type = typename kleene_type<Parser>::value_type;

    maybe<value_type> operator()(std::istream& in) const {

      const auto impl = parser >> [&](source_type&& first) {
        return kleene(parser) >> [first](value_type&& value) {
          value.emplace(value.begin(), std::move(first));
          return pure(value);
        };
      };

      return impl(in);
    }
    
  };


  template<class Parser>
  static plus_type<Parser> plus(Parser parser) { return {parser}; }

  template<class Parser>
  static plus_type<Parser> operator+(Parser parser) { return {parser}; }
  

  // lists with separators
  template<class Parser, class Separator>
  struct list_type {
    const Parser parser;
    const Separator separator;

    using source_type = typename traits<Parser>::value_type;
    using value_type = typename kleene_type<Parser>::value_type;
    
    maybe<value_type> operator()(std::istream& in) const {

      const auto impl = parser >> [&](source_type first) {
        return *(separator, parser) >> [first](value_type&& value) {
          value.emplace_front(std::move(first));
          return pure(value);
        };
      } | pure( value_type() );

      return impl(in);    
    }
  };


  template<class Parser, class Separator>
  static list_type<Parser, Separator> list(Parser parser, Separator separator) {
    return {parser, separator};
  }

  template<class Parser, class Separator>
  static list_type<Parser, Separator> operator%(Parser parser, Separator separator) {
    return {parser, separator};
  }
  

  // some quick debugging tools
  static std::size_t indent = 0;
  static const bool use_debug = false;
  
  template<class Parser, bool = use_debug>
  struct debug_type;

  
  template<class Parser>
  struct debug_type<Parser, true> {
    const std::string name;
    const Parser parser;
 
    using value_type = typename traits<Parser>::value_type;

    maybe<value_type> operator()(std::istream& in) const {
      for(std::size_t i = 0; i < indent; ++i) std::clog << ' ';
      
      std::clog << ">> " << name << " " << char(in.peek()) << std::endl;
      ++indent;
      const maybe<value_type> res = parser(in);
      --indent;

      if( res ) {
        for(std::size_t i = 0; i < indent; ++i) std::clog << ' ';
        std::clog << "<< " << name << std::endl;
      }
      return res;
    }
    
  };

  template<class Parser>
  struct debug_type<Parser, false> {
    
    const Parser parser;
    debug_type(const char*, const Parser& parser) : parser(parser) { }
    
    using value_type = typename traits<Parser>::value_type;
    
    maybe<value_type> operator()(std::istream& in) const {
      return parser(in);
    }
    
  };
  
  
  struct debug {
    const char* name;
    debug(const char* name) : name(name) { }
    
    template<class Parser>
    debug_type<Parser> operator>>=(const Parser& parser) const {
      return {name, parser};
    }
    
  };
  

}


#endif
