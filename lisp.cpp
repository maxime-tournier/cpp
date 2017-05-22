#include <iostream>
#include "parse.hpp"

#include <sstream>
#include <string>

#include <set>
#include <memory>
#include <vector>
#include <deque>
#include <cassert>
#include <fstream>

#include <unordered_map>
#include <map>

#include <readline/readline.h>
#include <readline/history.h>

#include "indices.hpp"
#include "ref.hpp"


namespace lisp {
 
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

    // TODO do we want this ?
    symbol() : iterator(table.end()) { }
    
    symbol(const string& value) 
      : iterator(table.insert(value).first) { }  

    symbol(const char* value) 
      : iterator(table.insert(value).first) { }  


    bool operator==(const symbol& other) const {
      return iterator == other.iterator;
    }
    
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
  struct value : variant<list, integer, real, symbol, ref<string>,
                         ref<lambda>, builtin > {
    using value::variant::variant;

    value(value::variant&& other)
      : value::variant( std::move(other))  { }

    value(const value::variant& other)
      : value::variant(other) { }

    
    list operator>>=(list tail) const {
      return make_ref<cell>(*this, tail);
    }
 
    struct ostream;

    // nil check
    explicit operator bool() const {
      return !is<list>() || get<list>();
    }

  }; 

  static const list nil;
  
  static std::ostream& operator<<(std::ostream& out, const value& self);  

  struct cell {

    value head;
    list tail;

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

  struct error : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  
  struct empty_list : error {
    empty_list() : error("empty list") { } 
  };

  static const list& unpack(const list& from) { return from; }
  
  template<class H, class ... Args>
  static const list& unpack(const list& from, H* to, Args*... args) {
    if(!from) throw empty_list();
    *to = from->head.cast<H>();
    return unpack(from->tail, args...);
  }


  static value& head(const list& x) {
    if(!x) throw empty_list();
    return x->head;
  }

  static const list& tail(const list& x) {
    if(!x) throw empty_list();
    return x->tail;
  }
  
  
  
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

    void operator()(const ref<string>& self, std::ostream& out) const {
      out << '"' << *self << '"';
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


  
  struct unbound_variable : error {
    unbound_variable(const symbol& s)
      : error("unbound variable: " + s.name()) { }
    
  };

  struct syntax_error : error {
    syntax_error(const std::string& s)
      : error("syntax error: " + s) { }
    
  };
   
  
  struct context {
    
    ref<context> parent;
    context(const ref<context>& parent = {}) : parent(parent) { }
    
    using locals_type = std::map< symbol, value >;
    locals_type locals;

    value& find(const symbol& name) {
      auto it = locals.find(name);
      if(it != locals.end()) return it->second;
      if(parent) return parent->find(name);
      throw unbound_variable(name);
    }

    context& operator()(const symbol& name, const value& expr) {
      locals.emplace( std::make_pair(name, expr) );
      return *this;
    }
    
  };


  template<class Iterator>
  static ref<context> extend(const ref<context>& parent, Iterator first, Iterator last) {
    ref<context> res = make_ref<context>(parent);
    res->locals.insert(first, last);
    
    return res;
  }




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
      
      try {
        list params;
        const list& rest = unpack(args, &params);

        std::vector<symbol> vars;
        
        symbol s;
        while(params) {
          params = unpack(params, &s);
          vars.push_back(s);
        }
        
        return make_ref<lisp::lambda>(ctx, std::move(vars), head(rest));
      } catch(empty_list& e) {
        throw syntax_error("lambda");        
      } catch(value::bad_cast& e) {
        throw syntax_error("symbol list expected for function parameters");
      } 

    }

    static value def(const ref<context>& ctx, const list& args) {
      
      struct fail { };
      
      try{
        symbol name;
        const list& rest = unpack(args, &name);
        
        const value val = eval(ctx, head(rest));
        auto res = ctx->locals.emplace( std::make_pair(name, val) );
        if(!res.second) res.first->second = val;

        // TODO nil?
        return nil;
      } catch( value::bad_cast& e) {
        throw syntax_error("symbol expected");
      } catch( empty_list& e) {
        throw syntax_error("def");
      }
      
    }


    static value cond(const ref<context>& ctx, const list& args) {
      try {
        for(const value& x : args) {
          const list& term = x.cast<list>();

          if( eval(ctx, head(term)) ) {
            return eval(ctx, head(tail(term)));
          }
        }
      } catch(value::bad_cast& e) {
        throw syntax_error("list of pairs expected");
      } catch( empty_list& e ){
        throw syntax_error("cond");
      } 
      
      return nil;
    }


    static value quote(const ref<context>& ctx, const list& args) {
      try {
        return head(args);
      } catch( empty_list& e ) {
        throw syntax_error("quote");
      }
    }




    // special forms
    static const std::map<symbol, type> table = {
      {"lambda", lambda},
      {"def", def},
      {"cond", cond},
      {"quote", quote}
    };

    static const auto table_end = table.end();
  }
  
  struct eval_visitor {

    static std::vector< value > stack;
    
    value operator()(const symbol& self, const ref<context>& ctx) const {
      return ctx->find(self);
    }

    value operator()(const list& self, const ref<context>& ctx) const {
      if(!self) throw error("empty list in application");

      const value& first = self->head;

      // special forms
      if(first.is<symbol>()) {
        auto it = special::table.find(first.get<symbol>());
        if(it != special::table_end) return it->second(ctx, self->tail);
      }
      

      // applicatives
      const value app = eval(ctx, first);

      const std::size_t start = stack.size();
      
      for(const value& x : self->tail) {
        stack.emplace_back( eval(ctx, x) );
      }
      
      const value res = apply(app, stack.data() + start, stack.data() + stack.size());
      stack.resize(start, nil);

      return res;
    }
    
    
    template<class T>
    value operator()(const T& self, const ref<context>& ctx) const {
      return self;
    }
    
  };


  std::vector<value> eval_visitor::stack;
  
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

      ref<context> sub = extend(self->ctx, pair_first, pair_last);
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





static parse::any<lisp::value> sexpr_parser() {
  
  using namespace parse;

  const auto alpha = debug("alpha") >>= chr<std::isalpha>();
  const auto alnum = debug("alnum") >>= chr<std::isalnum>();
  const auto space = debug("space") >>= chr<std::isspace>();
  
  const auto dquote = chr('"');
  
  // const auto endl = chr('\n');
  // const auto comment = (chr(';'), kleene(!endl), endl);

  const auto as_value = cast<lisp::value>();
  
  const auto real = debug("real") >>= 
    lit<lisp::real>() >> as_value;
  
  const auto integer = debug("integer") >>=
    lit<lisp::integer>() >> as_value;
  
  const auto symbol = debug("symbol") >>=
    no_skip(alpha >> [alnum](char first) {
        return *alnum >> [first](std::deque<char>&& chars) {
          chars.emplace_front(first);
          const lisp::string str(chars.begin(), chars.end());
          return pure<lisp::value>( lisp::symbol(str) );
        };
      });
    
    
  const auto string = debug("string") >>=
    no_skip( (dquote, *!dquote) >> [dquote](std::deque<char>&& chars) {
        ref<lisp::string> str = make_ref<lisp::string>(chars.begin(), chars.end());
        return dquote, pure<lisp::value>(str);
      });
  
  const auto atom = debug("atom") >>=
    string | symbol | integer | real;
    
  struct expr_tag;
  rec<lisp::value, expr_tag> expr;
  
  const auto list = debug("list") >>=
    no_skip( (chr('('), *space, expr % +space) >> [space](std::deque<lisp::value>&& terms) {
        return *space, chr(')'), pure<lisp::value>(lisp::make_list(terms));
      });
  
  expr = debug("expr") >>=
    atom | list;
  
  return expr;
}


namespace lisp {
  
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*)(Args... args), indices<I...> indices) {
    static Ret(*ptr)(Args...) = f;
    
    return [](const value* first, const value* last) -> value {
      if((last - first) != sizeof...(Args)) {
        throw error("argc");
      }
 
      return ptr( first[I].template cast<Args>() ... );
    };
  } 
 
  template<class F, class Ret, class ... Args, std::size_t ... I>
  static builtin wrap(const F& f, Ret (*ptr)(Args... args)) {
    return wrap(f, ptr, typename tuple_indices<Args...>::type() );
  }
  
  template<class F>
  static builtin wrap(const F& f) {
    return wrap(f, +f);
  }


  static value list_ctor(const value* first, const value* last) {
    list res;
    list* curr = &res;
    
    for(const value* it = first; it != last; ++it) {
      *curr = make_ref<cell>(*it, nil);
      curr = &(*curr)->tail;
    }
                                                 
    return res;
  }

  struct eq_visitor {

    template<class T>
    void operator()(const T& self, const value& other, bool& result) const {
      result = self == other.template get<T>();
    }
    
  };
  
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


int main(int argc, char** argv) {
  using namespace lisp;
  ref<context> ctx = make_ref<context>();

  (*ctx)
    ("add", +[](const value* first, const value* last) -> value {
      integer res = 0;
      while(first != last) {
        res += (first++)->cast<integer>();
      };
      return res;
    })
    ("sub", wrap([](integer x, integer y) -> integer { return x - y; }))
    ("mul", wrap([](integer x, integer y) -> integer { return x * y; }))
    ("div", wrap([](integer x, integer y) -> integer { return x / y; }))
    ("mod", wrap([](integer x, integer y) -> integer { return x % y; }))        
    
    ("list", list_ctor)
    ("print", +[](const value* first, const value* last) -> value {
      bool start = true;
      for(const value* it = first; it != last; ++it) {
        if(start) start = false;
        else std::cout << ' ';
        std::cout << *it;
      }
      std::cout << std::endl;
      return nil;
    })
    ("eq", +[](const value* first, const value* last) -> value {
      if(last - first < 2) throw error("argc");
      if(first[0].type() != first[1].type()) return nil;
      bool res;
      first[0].apply(eq_visitor(), first[1], res);
      if(!res) return nil;

      static symbol t("t");
      return t;
    })
    ;
    
   
  const auto parser = *sexpr_parser() >> [&](std::deque<value>&& exprs) {
    try{

      for(const value& e : exprs) {
        const value val = eval(ctx, e);
        if(val) {
          std::cout << val << std::endl; 
        }
      }
      
    } catch( error& e ) {
      std::cerr << "error: " << e.what() << std::endl;
    }
    
    return parse::pure(exprs);
  };

  if( argc > 1 ) {
    std::ifstream file(argv[1]);
    
    if(!parser(file)) {
      std::cerr << "parse error" << std::endl;
      return 1;
    }
    
  } else {
  
    read_loop([parser](std::istream& in) {
        return bool( parser(in) );
      });
  }
 
  return 0;
}
 
 
