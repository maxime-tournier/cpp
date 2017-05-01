#ifndef PARSE_HPP
#define PARSE_HPP

#include <istream>
#include <functional>

namespace parse {

  // istream position rollback helper
  struct point {
	std::ios::pos_type pos;
	
	point() {}
	
	point(std::istream& in)
	  : pos(in.tellg()) {
	    
	};
	
	void reset(std::istream& in) const {
	  in.clear();
	  in.seekg(pos);
	}
  };
  

  // sequence
  template<class LHS, class RHS>
  struct seq {
	LHS lhs;
	RHS rhs;
  };

  template<class LHS, class RHS>
  static std::istream& operator>>(std::istream& in, seq<LHS, RHS>& self) {
	point pos = in;
	
	if( in >> self.lhs ) {
	  if( in >> self.rhs ) {
		return in;
	  }
	}

	pos.reset(in);
	in.setstate(std::ios::failbit);
	return in;
  }
  
  

  template<class LHS, class RHS>
  static seq<LHS, RHS> operator,(const LHS& lhs, const RHS& rhs) {
	return {lhs, rhs};
  }

  
  // alternative
  template<class LHS, class RHS>
  struct alt {
	LHS lhs;
	RHS rhs;

  };

  template<class LHS, class RHS>
  static std::istream& operator>>(std::istream& in, alt<LHS, RHS>& self) {
	point pos = in;
	
	if( in >> self.lhs ) {
	  return in;
	}

	pos.reset(in);
	    
	if( in >> self.rhs ) {
	  return in;
	}

	pos.reset(in);

	in.setstate(std::ios::failbit);
	return in;
  }


  template<class LHS, class RHS>
  alt<LHS, RHS> operator|(const LHS& lhs, const RHS& rhs) {
	return {lhs, rhs};
  }

  

  // predicated-based character parsing
  template<class Pred>
  struct character {
	const Pred pred;

	struct negate {
	  const Pred pred;

	  bool operator()(char c) const {
		return !pred(c);
	  }

	};

	character<negate> operator!() const {
	  return {pred};
	}
	
  };
  

  template<class Pred>
  static std::istream& operator>>(std::istream& in, const character<Pred>& self) {
	char c;
	if(in >> c) {
	  if( self.pred(c)) {
		return in;
	  }
	  in.putback(c);
	  in.setstate(std::ios::failbit);
	}
	return in;
  }
  
  struct equals {
	const char expected;

	bool operator()(char c) const {
	  return c == expected;
	}
  };
  
  static character<equals> chr(char c) { return {{c}}; }
  
  template<class Pred>
  static character<Pred> chr(const Pred& pred) { return {pred}; }

  // convenience for using std::isalpha and friends
  template<int (*pred)(int) > struct predicate_adaptor {
    bool operator()(char c) const { return pred(c); }
  };

  template<int (*pred)(int)> static character<predicate_adaptor<pred>> chr() { return {}; }
  
  
  // kleene star
  template<class Parser>
  struct kleene {
	Parser parser;
  };

  template<class Parser>
  static std::istream& operator>>(std::istream& in, kleene<Parser>& self) {
	point pos;
	    
	do {
	  pos = point(in);
	} while(in >> self.parser);

	pos.reset(in);
	return in;
  }
  

  template<class Parser>
  static kleene< Parser > operator*(const Parser& parser) {
	return {parser};
  }
  

  template<class Parser>
  static seq<Parser, kleene<Parser> > operator+(const Parser& parser) {
	return {parser, {parser}};
  }
  
  
  // literal
  template<class T>
  struct lit {
	T value = {};
  };


  template<class T>
  static std::istream& operator>>(std::istream& in, lit<T>& self) {
	point pos;
	
	if( in >> self.value ) {
	  return in;
	}
	
	pos.reset(in);
	in.setstate(std::ios::failbit);
	return in;
  }



  
  // optional
  template<class Parser>
  struct option {
	Parser parser;
  };

  template<class Parser>
  static std::istream& operator>>(std::istream& in, option<Parser>& self) {
	point pos = in;
	if( in >> self.parser ) return in;
	pos.reset(in);
	return in;
  }
  
  template<class Parser>
  static option< Parser > operator~( const Parser& parser) {
	return { parser };
  }

    

  // apply callback on result
  template<class Parser, class OnMatch, class OnFail>
  struct apply {
	Parser parser;
	const OnMatch on_match = {};
	const OnFail on_fail = {};
  };
  
  template<class Parser, class OnMatch, class OnFail>
  static std::istream& operator>>(std::istream& in,
								  apply<Parser, OnMatch, OnFail>& self) {
	if(in >> self.parser) {
	  self.on_match(self.parser);
	} else {
	  self.on_fail(self.parser);
	}
  }

  
  struct noop {

	template<class T>
	void operator()(const T& ) const { };
	
  };


  template<class Parser, class OnMatch>
  apply<Parser, OnMatch, noop> on_match(const OnMatch& f, const Parser& parser= {} ) {
	return {parser, f, {}};
  }

  template<class Parser, class OnFail>
  apply<Parser, noop, OnFail> on_fail(const OnFail& f, const Parser& parser= {} ) {
	return {parser, {}, f};
  }


  // disable whitespace skipping
  template<class Parser>
  struct no_skip_ws {
	Parser parser;
  };




  template<class Parser>
  static std::istream& operator>>(std::istream& in, no_skip_ws<Parser>& self) {
	if(in.flags() & std::ios::skipws) {
	  return in >> std::noskipws >> self.parser >> std::skipws;
	}
	    
	return in >> self.parser;
  }


  template<class Parser>
  static no_skip_ws< Parser > no_skip( const Parser& parser) {
	return {parser};
  }
  

  // recursive parsers with a given tag all share the same implementation

  template<class A>
  struct tag {
    friend constexpr int adl_flag(tag);
  };
  
  template<class A>
  struct declare_flag : tag<A> { };
  
  template <class A>
  struct set_flag {
    friend constexpr int adl_flag(tag<A>) {
      return 0;
    }
  };

  template<class A, int = adl_flag( tag<A>() )>
  constexpr bool is_set(int) {
    return true;
  }
  
  template<class A>
  constexpr bool is_set (...) {
    return false;
  }

  
  
  template<class Tag>
  class any {

    using impl_type = std::istream& (*)(std::istream& in);
    static impl_type impl;

    using trigger = declare_flag<any>;
    
  public:


    
    template<class Parser>
	any& operator=(Parser&& parser) {
      static_assert(!is_set< any >(0), "redefined parser");
      sizeof(set_flag<any>);
      
      static typename std::decay<Parser>::type upvalue = std::forward<Parser>(parser);
      impl = [](std::istream& in) -> std::istream& { 
        return in >> upvalue;
      };
      
	  return *this;
	}
    
    

	friend std::istream& operator>>(std::istream& in, any& self) {
      static_assert( is_set<any>(0), "undefined parser" );
      
	  return impl(in);
	}

  };

  template<class Tag>
  typename any<Tag>::impl_type any<Tag>::impl = nullptr;
  


  template<class Parser>
  struct to_string {
    Parser parser;
    std::string value;
  };

  template<class Parser>
  static to_string<Parser> tos(const Parser& parser) {
    return {{parser}, {}};
  }
  
  template<class Parser>
  static std::istream& operator>>(std::istream& in, to_string<Parser>& self) {
    const std::ios::pos_type start = in.tellg();

    if(in >> self.parser) {
      const std::size_t size = in.tellg() - start;
      self.value.resize(size);

      // rewind and read to string
      in.seekg(start);
      in.read(&self.value[0], size);
	}  

    return in;
  }
}


#endif
