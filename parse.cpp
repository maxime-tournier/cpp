// -*- compile-command: "c++ -std=c++11 parse.cpp -o parse" -*-

#include <iostream>
#include "parse.hpp"


#include <sstream>

#include <set>
#include <memory>

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
  std::size_t index;
  using storage_type = typename std::aligned_union<0, T...>::type;
  storage_type storage;
  
  using select_type = impl::select<0, T...>;

  template<class U, class Variant, class Visitor, class ... Args>
  static void apply_thunk(Variant& self, Visitor&& visitor, Args&& ... args) {
    std::forward<Visitor>(visitor)(self.template get<U>(), std::forward<Args>(args)...);
  }


  template<class U>
  void construct(U&& value) {
    new (&storage) typename std::decay<U>::type(std::forward<U>(value));
  }
  
  struct copy {
    template<class U>
    void operator()(U& self, const variant& other) const {
      new (&self) U(other.template get<U>());
    }
  };

  struct move {
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

  
  template<class U>
  variant(U&& value)
    : index( select_type::index(value) ) {
    construct( select_type::cast(value) );
  }

  variant(const variant& other)
    : index(other.index) {
    apply( copy(), other );
  }

  variant(variant&& other)
    : index(other.index) {
    apply( move(), std::move(other) );
  }
  
  ~variant() {
    apply( destruct() );
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


class symbol {
  using table_type = std::set<std::string>;
  static table_type table;

  using iterator_type = table_type::iterator;
  iterator_type iterator;
public:

  symbol(const std::string& value) : iterator(table.insert(value).first) { }  
  
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


int main(int, char** ) {

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
   
  while(std::cin) {
	std::string line;
    
    std::cout << "> ";
	std::getline(std::cin, line);
	 
	std::stringstream ss(line);
	if(ss >> parser) {
	  std::cout << "parse succeded" << std::endl;
	} else if(!std::cin.eof()) {
	  std::cerr << "parse error" << std::endl;
	}
  }
  
}
 
 
