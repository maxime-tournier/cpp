#ifndef PARSE_HPP
#define PARSE_HPP

#include <istream>

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


  // template<class Parser>
  // static std::istream& operator>>(std::istream& in, no_skip_ws<Parser>&& self) {
  // 	if(in.flags() & std::ios::skipws) {
  // 	  return in >> std::noskipws >> self.parser >> std::skipws;
  // 	}
	    
  // 	return in >> self.parser;
  // }
  
  
  template<class Parser>
  static no_skip_ws< Parser > no_skip( const Parser& parser) {
	return {parser};
  }
  

  // recursive parsers with a given tag all share the same implementation
  template<class Tag>
  struct rec {
	using impl_type = std::function< std::istream& (std::istream&) >;

	static impl_type& impl() {
	  static impl_type impl;
	  return impl;
	}

	rec() { }
	
	template<class Parser>
	rec& operator=(Parser&& parser) {
	  impl() = [parser](std::istream& in) mutable -> std::istream& {
		return in >> parser;
	  };
	  return *this;
	}

	friend std::istream& operator>>(std::istream& in, rec& self) {
	  if(!impl()) {
		throw std::runtime_error("empty parser");
	  }
	  return impl()(in);
	}

  };


    
}


#endif
