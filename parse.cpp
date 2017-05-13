#include <iostream>
#include "parse.hpp"

#include <sstream>

#include <set>
#include <memory>
#include <vector>
#include <deque>
#include <cassert>

#include <unordered_map>
#include <map>

#include <readline/readline.h>
#include <readline/history.h>


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

  template<class U, class Ret, class Variant, class Visitor, class ... Args>
  static Ret call_thunk(Variant& self, Visitor&& visitor, Args&& ... args) {
    return std::forward<Visitor>(visitor)(self.template get<U>(), std::forward<Args>(args)...);
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

  struct bad_cast : std::runtime_error {
    bad_cast() : std::runtime_error("bad_cast") { }
  };
  
  template<class U>
  U& get() {
    U& res = reinterpret_cast<U&>(storage);
    if( select_type::index(res) != index ) throw bad_cast();
    return res;
  }

  template<class U>
  const U& get() const {
    const U& res = reinterpret_cast<const U&>(storage);
    if( select_type::index(res) != index ) {
      std::clog << select_type::index(res) << " vs. " << index << std::endl;
      throw std::runtime_error("bad cast");
    }
    return res;
  }

  template<class U, int R = select_type::index( *(U*)0 )>
  bool is() const {
    return R == index;
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
  template<class U, class = decltype( select_type::index( std::declval<const U&>() ) ) >
  variant(const U& value)
    : index( select_type::index(value) ) {
    construct( select_type::cast(value) );
  }
  
  
  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) {
    using thunk_type = void (*)(variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      call_thunk<T, void, variant, Visitor, Args...>...
    };
    
    thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }

  template<class Visitor, class ... Args>
  void apply(Visitor&& visitor, Args&& ... args) const {
    using thunk_type = void (*)(const variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      call_thunk<T, void, const variant, Visitor, Args...>...
    };
    
    thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
  }


  template<class Visitor, class ... Args>
  variant map(Visitor&& visitor, Args&& ... args) const {
    using thunk_type = variant (*)(const variant& self, Visitor&& visitor, Args&& ... args);

    static constexpr thunk_type thunk[] = {
      call_thunk<T, variant, const variant, Visitor, Args...>...
    };
    
    return thunk[index](*this, std::forward<Visitor>(visitor), std::forward<Args>(args)...);
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
  struct pure_type {
    using value_type = T;

    const value_type value;
  
    maybe<value_type> operator()(std::istream& in) const {
      return value;
    }
  
  };

  template<class T>
  static pure_type<T> pure(T value) { return {value}; }


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

    using domain_type = typename Parser::value_type;
    
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


  template<class Parser>
  struct kleene_type {
    const Parser parser;

    using source_type = typename Parser::value_type;
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



  // literal
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


  // boost::spirit style
  static struct {

    template<class Parser>
    no_skip_type<Parser> operator[](Parser parser) const {
      return {parser};
    }
  } no_skip;



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
  

  template<class T>
  struct any : std::function< maybe<T>(std::istream& in) >{
    using value_type = T;
    
    using any::function::function;
  };

  // hides std::ref
  template<class T>
  static ref_type<any<T>> ref(any<T>& self) { return {self}; }

  template<class T>
  static ref_type<any<T>> ref(const any<T>& self) { return {self}; }    
  
  
  template<class T>
  struct cast {
    
    template<class U>
    pure_type<T> operator()(U u) const {
      return pure<T>(u);
    } 
    
  };


  template<class Parser>
  struct plus_type {
    const Parser parser;

    using source_type = typename Parser::value_type;
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
  

  template<class Parser, class Separator>
  struct list_type {
    const Parser parser;
    const Separator separator;

    using source_type = typename Parser::value_type;
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
  
  
}





template<class F>
static void read_loop(const F& f) {

  while( const char* line = readline("> ") ) {
    add_history(line);

	std::stringstream ss(line);

	if( f(ss) ) {
      // success
	} else if(!std::cin.eof()) {
	  std::cerr << "parse error" << std::endl;
	}
    
  }
  
};


namespace sexpr {

  template<class T>
  using ref = std::shared_ptr<T>;

  struct value;
  
  struct cell;
  using list = ref<cell>;

  struct context;
  struct lambda;
  
  using string = std::string;
  using integer = long;
  using real = double;

  // TODO maybe not const ?
  // TODO pass ctx ?
  using builtin = value (*)(const value* first, const value* last);
  
  class symbol {
    using table_type = std::set<string>;
    static table_type table;

    using iterator_type = table_type::iterator;
    iterator_type iterator;
  public:

    symbol(const string& value) 
      : iterator(table.insert(value).first) { }  

    symbol(const char* value) 
      : iterator(table.insert(value).first) { }  

    
    const std::string& name() const { return *iterator; }

    bool operator<(const symbol& other) const {
      return &(*iterator) < &(*other.iterator);
    }
    
  };

  symbol::table_type symbol::table;


  template<class Container>
  static list make_list(const Container& container) {
    list res;
    
    for(auto it = container.rbegin(), end = container.rend();
        it != end; ++it) {
      res = *it >>= res;
    }
    
    return res;
  }


  // TODO ref<string> ?
  struct value : variant<list, integer, real, symbol, string,
                         ref<lambda>, builtin > {
    using value::variant::variant;

    value(value::variant&& other)
      : value::variant( std::move(other))  { }

    value(const value::variant& other)
      : value::variant(other) { }

    
    list operator>>=(list tail) const {
      return std::make_shared<cell>(*this, tail);
    }

    struct ostream;
  };

  static std::ostream& operator<<(std::ostream& out, const value& self);  

  struct cell {

    value head;
    list tail = 0;

    cell(const value& head,
         const list& tail)
      : head(head),
        tail(tail) {

    }
    
    struct iterator {
      cell* ptr;

      value& operator*() const { return ptr->head; }
      bool operator!=(iterator other) const { return ptr != other.ptr; }
      iterator& operator++() { ptr = ptr->tail.get(); return *this; }
    };

  };
  
  static cell::iterator begin(const list& self) { return { self.get() }; }
  static cell::iterator end(const list& self) { return { nullptr }; }
  
  struct value::ostream {

    void operator()(const list& self, std::ostream& out) const {
      bool first = true;
      out << '(';
      for(const value& x : self) {
        if(first) first = false;
        else out << ' ';
        out << x;
      }
      out << ')';
    }

    void operator()(const symbol& self, std::ostream& out) const {
      out << self.name();
    }

    void operator()(const string& self, std::ostream& out) const {
      out << '"' << self << '"';
    }

    void operator()(const builtin& self, std::ostream& out) const {
      out << "#<builtin>";
    }

    void operator()(const ref<lambda>& self, std::ostream& out) const {
      out << "#<lambda>";
    }
    
      
    template<class T>
    void operator()(const T& self, std::ostream& out) const {
      out << self;
    }
    
  };

  static std::ostream& operator<<(std::ostream& out, const value& self) {
    self.apply( value::ostream(), out );
    return out;
  }


  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };
  
  struct unbound_variable : error {
    unbound_variable(const symbol& s)
      : error("unbound variable: " + s.name()) { }
    
  };

  struct syntax_error : error {
    syntax_error(const std::string& s)
      : error("syntax error: " + s) { }
    
  };
  
  
  struct context : std::enable_shared_from_this<context> {
    
    ref<context> parent;

    using locals_type = std::map< symbol, value >;
    locals_type locals;

    value& find(const symbol& name) {
      auto it = locals.find(name);
      if(it != locals.end()) return it->second;
      if(parent) return parent->find(name);
      throw unbound_variable(name);
    }

    template<class Iterator>
    ref<context> extend(Iterator first, Iterator last) {
      ref<context> res = std::make_shared<context>();

      res->parent = shared_from_this();
      res->locals.insert(first, last);

      return res;
    }

    context& operator()(const symbol& name, const value& expr) {
      locals.emplace( std::make_pair(name, expr) );
      return *this;
    }
    
  };


  struct lambda {

    ref<context> ctx;
    using args_type = std::vector<symbol>;
    args_type args;
    value body;


    lambda(const ref<context>& ctx,
           args_type&& args,
           const value& body)
      : ctx(ctx),
        args(std::move(args)),
        body(body) {

    }
    
    
  };


  static value eval(const ref<context>& ctx, const value& expr);
  static value apply(const value& app, const value* first, const value* last);


  namespace special {
    using type = value (*)(const ref<context>&, const list& args);
    
    static value lambda(const ref<context>& ctx, const list& args) {
      auto fail = [] { throw syntax_error("lambda"); };

      try {
        if(!args) fail();
        if(!args->tail) fail();
        
        const value& body = args->tail->head;

        std::vector<symbol> vars;
        for(const value& v : args->head.get<list>() ) {
          vars.push_back(v.get<symbol>());
        }
        
        return std::make_shared<sexpr::lambda>(ctx, std::move(vars), body);
        
      } catch(value::bad_cast& e) {
        fail();
      }
      throw;
    }

    static value def(const ref<context>& ctx, const list& args) {
      if(args && args->head.is<symbol>() && args->tail && !args->tail->tail) {
        const value& val = eval(ctx, args->tail->head);
        auto res = ctx->locals.emplace( std::make_pair(args->head.get<symbol>(), val) );
        if(!res.second) res.first->second = val;
        return list();
      } else {
        throw syntax_error("def");
      }
    }

  }
  
  struct eval_visitor {

    value operator()(const symbol& self, const ref<context>& ctx) const {
      return ctx->find(self);
    }

    value operator()(const list& self, const ref<context>& ctx) const {
      if(!self) throw error("empty list in application");

      const value& first = self->head;

      // special forms
      static const std::map<symbol, special::type> table = {
        {"lambda", special::lambda},
        {"def", special::def}
      };

      if(first.is<symbol>()) {
        auto it = table.find(first.get<symbol>());
        if(it != table.end()) return it->second(ctx, self->tail);
      }
      

      // applicatives
      const value app = eval(ctx, first);
      
      // TODO eval args to call stack ?
      std::vector<value> args;      
      for(const value& x : self->tail) {
        args.emplace_back( eval(ctx, x) );
      }

      return apply(app, args.data(), args.data() + args.size());
    }
    
    
    template<class T>
    value operator()(const T& self, const ref<context>& ctx) const {
      return self;
    }
    
  };

  
  static value eval(const ref<context>& ctx, const value& expr) {
    return expr.map( eval_visitor(), ctx );
  }


  struct apply_visitor {

    value operator()(const ref<lambda>& self, const value* first, const value* last) {

      struct iterator {
        lambda::args_type::iterator it_name;
        const value* it_arg;

        iterator& operator++() {
          ++it_name, ++it_arg;
          return *this;
        }

        std::pair<symbol, const value&> operator*() const {
          return {*it_name, *it_arg};
        }
        
        bool operator!=(const iterator& other) const {
          const bool cond_name = it_name != other.it_name;
          const bool cond_arg = it_arg != other.it_arg;

          if(cond_name ^ cond_arg) throw error("arg count");

          return cond_name || cond_arg;
        }
        
      };

      iterator pair_first = {self->args.begin(), first};
      iterator pair_last = {self->args.end(), last};

      ref<context> sub = self->ctx->extend(pair_first, pair_last);
      return eval(sub, self->body);
    }

    value operator()(const builtin& self, const value* first, const value* last) {
      return self(first, last);      
    }
    

    template<class T>
    value operator()(const T& self, const value* first, const value* last) {
      throw error("cannot apply");
    }
    
  };

  
  static value apply(const value& app, const value* first, const value* last) {
    return app.map(apply_visitor(), first, last);
  }
  
  
}





static monad::any<sexpr::value> sexpr_parser() {
  
  using namespace monad;

  const auto alpha = chr<std::isalpha>();
  const auto alnum = chr<std::isalnum>();
  const auto space = chr<std::isspace>(); 

  const auto endl = chr('\n');
  const auto dquote = chr('"');
  
  auto comment = (chr(';'), kleene(!endl), endl);

  auto as_value = cast<sexpr::value>();
  
  auto real = lit<sexpr::real>() >> as_value;
  auto integer = lit<sexpr::integer>() >> as_value;
  
  auto symbol = no_skip[ alpha >> [alnum](char first) {
      return *alnum >> [first](std::deque<char>&& chars) {
        chars.emplace_front(first);
        const sexpr::string str(chars.begin(), chars.end());
        return pure<sexpr::value>( sexpr::symbol(str) );
      };
    }];
  
    
  auto string = no_skip[ (dquote, *!dquote) >> [dquote](std::deque<char>&& chars) {
      sexpr::string str(chars.begin(), chars.end());
      return dquote, pure<sexpr::value>(str);
    }];
    
  auto atom = string | symbol | integer | real;
    
  any<sexpr::value> expr;
    
  auto list = no_skip[ (chr('('), *space, ref(expr) % +space)
                       >> [space](std::deque<sexpr::value>&& terms) {
      return *space, chr(')'), pure<sexpr::value>(sexpr::make_list(terms));
    }];
  
  expr = atom | list;

  return expr;
}






int main(int, char** ) {

  using namespace sexpr;
  ref<context> ctx = std::make_shared<context>();

  (*ctx)
    ("add", [](const value* first, const value* last) -> value {
      integer res = 0;
      while(first != last) {
        res += (first++)->get<integer>();
      };
      return res;
    });
  
  
  const auto parser = sexpr_parser() >> [&](value&& expr) {
    try{
      std::cout << eval(ctx, expr) << std::endl;
    } catch( error& e ) {
      std::cerr << "error: " << e.what() << std::endl;
    }
    return monad::pure(expr);
  };
  
  read_loop([parser](std::istream& in) {
      return bool( parser(in) );
    });
  
  return 0;
}
 
 
