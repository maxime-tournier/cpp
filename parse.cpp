// -*- compile-command: "c++ -std=c++11 parse.cpp -o parse" -*-

#include <iostream>
#include "parse.hpp"


#include <sstream>

#include <set>
#include <memory>
#include <vector>


namespace impl {
  template<std::size_t start, class ... T>
  struct select;

  template<std::size_t start, class H, class ... T>
  struct select<start, H, T...> : select<start + 1, T...> {

    using select<start + 1, T...>::index;
    static constexpr std::size_t index(const H& ) { return start; }

    using select<start + 1, T...>::cast;
    static const H& cast(const H& value) { return value; }
    static H&& cast(H&& value) { return std::move(value); }    
    
  };

  template<std::size_t start> struct select<start> {
    static void index();
    static void cast();    
  };
}


template<class ... T>
class variant {

  using storage_type = typename std::aligned_union<0, T...>::type;
  storage_type storage;

  using index_type = std::size_t;
  index_type index;
  
  using select_type = impl::select<0, T...>;

  template<class U, class Variant, class Visitor, class ... Args>
  static void apply_thunk(Variant& self, Visitor&& visitor, Args&& ... args) {
    std::forward<Visitor>(visitor)(self.template get<U>(), std::forward<Args>(args)...);
  }


  template<class U>
  void construct(U&& value) {
    new (&storage) typename std::decay<U>::type(std::forward<U>(value));
  }
  
  struct copy_construct {
    template<class U>
    void operator()(U& self, const variant& other) const {
      new (&self) U(other.template get<U>());
    }
  };


  struct copy {
    template<class U>
    void operator()(U& self, const variant& other) const {
      self = other.template get<U>();
    }
  };


  struct move {
    template<class U>
    void operator()(U& self, variant&& other) const {
      self = std::move(other.template get<U>());
    }
  };
  
  
  struct move_construct {
    template<class U>
    void operator()(U& self, variant&& other) const {
      new (&self) U(std::move(other.template get<U>()));
    }
  };


  
  struct destruct {
    template<class U>
    void operator()(U& self) const {
      self.~U();
    }
  };


public:
  std::size_t type() const { return index; }
  
  template<class U>
  U& get() {
    U& res = reinterpret_cast<U&>(storage);
    if( select_type::index(res) != index ) throw std::runtime_error("bad cast");
    return res;
  }

  template<class U>
  const U& get() const {
    const U& res = reinterpret_cast<const U&>(storage);
    if( select_type::index(res) != index ) throw std::runtime_error("bad cast");
    return res;
  }


  
  variant(const variant& other)
    : index(other.index) {
    apply( copy_construct(), other );
  }

  variant(variant&& other)
    : index(other.index) {
    apply( move_construct(), std::move(other) );
  }
  
  ~variant() {
    apply( destruct() );
  }


  variant& operator=(const variant& other) {
    if(type() == other.type()) {
      apply( copy(), other );
    } else {
      apply( destruct() );
      apply( copy_construct(), other );
    }
    
    return *this;
  }


  variant& operator=(variant&& other) {
    if(type() == other.type()) {
      apply( move(), std::move(other) );
    } else {
      apply( destruct() );
      apply( move_construct(), std::move(other) );
    }
    
    return *this;
  }
  
  
  
  // TODO move constructors from values ?
  template<class U>
  variant(const U& value)
    : index( select_type::index(value) ) {
    construct( select_type::cast(value) );
  }
  
  
  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) {
    using thunk_type = void (*)(variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      apply_thunk<T, variant, Visitor, Args...>...
    };
    
    thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }

  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) const {
    using thunk_type = void (*)(const variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      apply_thunk<T, const variant, Visitor, Args...>...
    };
    
    thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }

  
}; 


struct none { };

template<class T>
struct maybe : public variant<none, T> {
  using variant<none, T>::variant;

  explicit operator bool() const {
    return this->type();
  }
  
};

namespace monad {

  // monadic return
  template<class T>
  struct result_type {
    using value_type = T;

    const value_type value;
  
    maybe<value_type> operator()(std::istream& in) const {
      return value;
    }
  
  };

  template<class T>
  static result_type<T> result(T value) { return {value}; }


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
  static lazy_type<F> lazy(F f) { return {f}; }


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

    using source_type = typename Parser::value_type;
    
    using result_type = typename std::result_of< F(source_type) >::type;
    using value_type = typename result_type::value_type;

    maybe<value_type> operator()(std::istream& in) const {
      parse::point pos(in);
      
      maybe<source_type> value = parser(in);

      if(!value) return none();

      const maybe<value_type> res = f( std::move(value.template get<source_type>() ) )(in);
      if(res) return res;

      // backtrack 
      pos.reset(in);
      // in.setstate(std::ios::failbit);
      return none();
    }
  
  };


  template<class Parser, class F>
  static bind_type<Parser, F> bind(Parser parser, F f) {
    return {parser, f};
  }


  template<class Parser, class F>
  static bind_type<Parser, F> operator>>(Parser parser, F f) {
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

  
  
  // alternatives
  template<class LHS, class RHS>
  struct alt_type {
    const LHS lhs;
    const RHS rhs;

    static_assert( std::is_same< typename LHS::value_type, typename RHS::value_type>::value );
    
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


  template<class Parser>
  struct kleene_type {
    const Parser parser;

    using source_type = typename Parser::value_type;
    using value_type = std::vector< source_type >;

    maybe<value_type> operator()(std::istream& in) const {
      value_type res;
      
      while( maybe<source_type> x = parser(in) ) {
        res.push_back(x.template get<source_type>() );
      }

      return res;
    }

  };


  template<class Parser>
  static kleene_type<Parser> kleene(Parser parser) {
    return {parser};
  }

  template<class Parser>
  static kleene_type<Parser> operator*(Parser parser) {
    return {parser};
  }






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

    using value_type = char;

    maybe<value_type> operator()(std::istream& in) const {
      char c;
      if(in >> c) {
        if( pred(c) ) return c;
        in.putback(c);
        // in.setstate(std::ios::failbit);
      }
      
      return none();
    }

  };
  
  struct equals {
	const char expected;

	bool operator()(char c) const {
      // std::clog << "got: '" << c << "', expecting: '" << expected << "'" << std::endl;
	  return c == expected;
	}
  };
  
  static character<equals> chr(char c) { return {{c}}; }

  template<int (*pred)(int) > struct adaptor {
    bool operator()(char c) const { return pred(c); }
  };

  template<int (*pred)(int)> static character<adaptor<pred>> chr() { return {}; }



  // literal
  template<class T>
  struct lit {

    using value_type = T;
    
    maybe<value_type> operator()(std::istream& in) const {
      parse::point pos(in);
      
      value_type value;
      if( in >> value ) {
        return value;
      }
	
      pos.reset(in);
      // in.setstate(std::ios::failbit);
      return none();
    }
   
  };



  template<class Parser>
  struct no_skip_type {
    const Parser parser;

    using value_type = typename Parser::value_type;

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
  static no_skip_type< Parser > no_skip(Parser parser) {
	return {parser};
  }
  
  template<class T>
  struct any : std::function< maybe<T>(std::istream& in) > {
    using std::function< maybe<T>(std::istream&) >::function;

    using value_type = T;
    
  };

  template<class Parser>
  struct ref_type {
    const Parser& parser;

    using value_type = typename Parser::value_type;

    maybe<value_type> operator()(std::istream& in) const {
      return parser(in);
    }
    
  };


  template<class Parser>
  static ref_type<Parser> ref(const Parser& parser) { return { parser }; }

  template<class Parser>
  static ref_type<Parser> operator&(const Parser& parser) { return { parser }; }
  
  
  template<class T>
  struct cast {
    
    template<class U>
    result_type<T> operator()(U u) const {
      return result<T>(u);
    }
    
  };


  template<class Parser>
  struct plus_type {
    const Parser parser;

    using source_type = typename Parser::value_type;
    using value_type = typename kleene_type<Parser>::value_type;

    maybe<value_type> operator()(std::istream& in) const {

      const auto impl = parser >> [&](source_type&& first) {
        return *parser >> [first](value_type&& value) {
          value.emplace(value.begin(), std::move(first));
          return result(value);
        };
      };

      return impl(in);
    }
    
  };


  template<class Parser>
  static plus_type<Parser> plus(Parser parser) { return {parser}; }

  template<class Parser>
  static plus_type<Parser> operator+(Parser parser) { return {parser}; }
  

  template<class Parser, class Separator>
  struct list_type {
    const Parser parser;
    const Separator separator;

    using source_type = typename Parser::value_type;
    using value_type = typename kleene_type<Parser>::value_type;
    
    maybe<value_type> operator()(std::istream& in) const {

      const auto impl = parser >> [&](source_type first) {
        return *(separator, parser) >> [first](value_type&& value) {
          value.emplace(value.begin(), std::move(first));
          return result(value);
        };
        } | result( value_type() );

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

  
}



class symbol {
  using table_type = std::set<std::string>;
  static table_type table;

  using iterator_type = table_type::iterator;
  iterator_type iterator;
public:

  symbol(const std::string& value) 
    : iterator(table.insert(value).first) { }  
  
};



int test() {

  struct cell;
  using list = std::shared_ptr<cell>;

  using string = std::string;
  using value = variant<long, double, symbol, string, list >;

  struct cell {
    value head;
    list tail = 0;
  };
  
  value x = 1.0f;
  
}


template<class F>
static void repl(const F& f) {

  while(std::cin) {
	std::string line;
    
    std::cout << "> ";
	std::getline(std::cin, line);

	std::stringstream ss(line);

	if( f(ss) ) {
	  std::cout << "parse succeded" << std::endl;
	} else if(!std::cin.eof()) {
	  std::cerr << "parse error" << std::endl;
	}
    
  }

};


using string = std::string;
struct value;

struct value : variant<long, double, symbol, std::vector<value> > {
  using value::variant::variant;
};


int main(int, char** ) {

  {
    using namespace monad;

    auto alpha = chr<std::isalpha>();
    auto alnum = chr<std::isalnum>();
    auto space = chr<std::isspace>(); 

    auto endl = chr('\n');
    auto comment = (chr(';'), *!endl, endl);

    auto real = lit<double>();
    auto integer = lit<long>();
    auto dquote = chr('"');

    // auto symbol = no_skip( alpha >> *alnum) ) );
    
    auto string = no_skip( (dquote,
                            (*!dquote) >> [dquote](std::vector<char>&& value) {
                              return dquote, result(std::move(value));
                            }));
    
    auto atom = real >> cast<value>() | integer >> cast<value>();

    any<value> expr;

    auto list = no_skip( (chr('('), *space, (&expr % +space) >> [space](std::vector<value>&& x) {
          return *space, chr(')'), result(x);
        }) );


    expr = atom | list >> cast<value>();
    
    auto parser = list;
    
    repl([&](std::istream& in) {
        return bool(parser(in));
      });

    return 0;
  }
  
  using namespace parse;
    
  auto alpha = chr<std::isalpha>();
  auto alnum = chr<std::isalnum>();
  auto space = chr<std::isspace>(); 
    
  auto endl = chr('\n');
  auto comment = (chr(';'), *!endl, endl);

  auto real = lit<double>();
  auto integer = lit<int>();

  auto dquote = chr('"');

  // TODO escaped strings
  auto string = tos( no_skip( (dquote,
                               *!dquote,
                               dquote)) );
  
  auto symbol = tos( no_skip( (alpha, *alnum) ) );

  auto atom = symbol | string | real | integer;
  
  struct expr_tag;
  any<expr_tag> expr;
  
  auto list = no_skip( (chr('('),
                        *space,
                        ~( expr, *(+space, expr) ),
                        *space,
                        chr(')' )) );
     
  expr = atom | list;
                      
  auto parser = expr;

  repl([&](std::istream& in) {
      return bool(in >> parser);
    });
  
}
 
 
