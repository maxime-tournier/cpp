#include "ast.hpp"
#include "sexpr.hpp"
#include "either.hpp"
#include "repl.hpp"

#include <map>
#include <functional>

namespace ast {

expr check(sexpr e);

template<class T>
struct success {
  T value;
  sexpr::list rest;
};


template<class T>
using result = either<std::string, success<T>>;

template<class T>
static result<T> make_success(T value, sexpr::list rest) {
  return success<T>{value, rest};
}

static const auto pop = [](sexpr::list list) -> result<sexpr> {
  if(list) {
    return make_success(list->head, list->tail);
  }
  
  return std::string("empty list");
};

static const auto empty = [](sexpr::list list) -> result<unit> {
  if(list) {
    return std::string("non-empty list");
  }
  
  return make_success(unit{}, list);
};

template<class T>
static auto expect(sexpr self) {
  return [self](sexpr::list rest) -> result<T> {
    return match(self,
                 [=](T expected) -> result<T> {
                   return make_success(expected, rest);
                 },
                 [](auto) -> result<T> {
                   return std::string("type error");
                 });
  };
}


static const auto bind = [](auto lhs, auto func) {
  return [=](sexpr::list args) {
    return lhs(args) >>= [=](auto head) {
      return func(head.value)(head.rest);
    };    
  };
};

static const auto map = [](auto lhs, auto func) {
  return [=](sexpr::list args) {
    return lhs(args) >>= [=](auto head) {
      return make_success(func(head.value), head.rest);
    };    
  };
};

static const auto pure = [](auto value) {
  return [=](sexpr::list rest) { return make_success(value, rest); };
};



template<class LHS, class Func>
static auto operator>>=(LHS lhs, Func func) { return bind(lhs, func); }

template<class LHS, class RHS>
static auto operator>>(LHS lhs, RHS rhs) {
  return lhs >>= [rhs](auto) {
    return rhs;
  };
}

template<class LHS, class Func>
static auto operator|=(LHS lhs, Func func) { return map(lhs, func); }

static const auto coproduct = [](auto lhs, auto rhs) {
  return [=](sexpr::list args) {
    if(auto res = lhs(args)) {
      return res;
    } else return rhs(args);
  };
};

template<class LHS, class RHS>
static auto operator|(LHS lhs, RHS rhs) {
  return coproduct(lhs, rhs);
}

template<class T, class Def>
struct fixpoint {
  const Def def;

  result<T> operator()(sexpr::list args) const {
    return def(*this)(args);
  };
};


template<class T, class Def>
static fixpoint<T, Def> fix(Def def) { return {def}; }


static const auto run = [](auto parser, sexpr::list args) {
  if(auto result = parser(args)) {
    return result.right().value;
  } else {
    throw std::runtime_error(result.left());
  }
};

////////////////////////////////////////////////////////////////////////////////
static const auto check_args = fix<list<var>>([](auto self) {
  return (empty >> pure(list<var>{}))
    | ((pop >>= expect<symbol>) >>= [=](symbol name) {
      return self >>= [=](list<var> tail) {
        return pure(var{name} %= tail);
      };
    });
});
                            
static const auto check_fn =
  (pop >>= expect<sexpr::list>) >>= [](sexpr::list args) {
    return pop >>= [=](sexpr body) {
      return [=](sexpr::list) -> result<expr> {
        return (check_args |= [=](list<var> args) -> expr {
          return foldr(args, check(body), [](var arg, expr body) -> expr {
            return abs{arg, body};
          });
        })(args);
      };
    };
  };

using special_type = std::function<result<expr>(sexpr::list)>;

static const std::map<symbol, special_type> special = {
  {"fn", check_fn}
};

static expr check_app(sexpr func, sexpr::list args) {
  return foldl(check(func), args, [](expr func, sexpr arg) -> expr {
    return app{func, check(arg)};
  });
}

expr check(sexpr e) {
  return match(
      e,
      [](auto self) -> expr { return lit{self}; },
      [](symbol self) -> expr { return var{self}; },
      [](sexpr::list self) -> expr {
        if(!self) {
          throw std::runtime_error("empty list in application");
        }

        return match(
            self->head,
            [=](symbol first) -> expr {
              const auto it = special.find(first);
              if(it != special.end()) {
                return run(it->second, self->tail);
              }
              return check_app(first, self->tail);
            },
            [=](sexpr func) -> expr { return check_app(func, self->tail); });
      });
}

}


template<class Exception, class Cont>
static auto rethrow_as(Cont cont) try {
  return cont();
} catch(std::exception& e) {
  throw Exception(e.what());
}


struct parse_error: std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct syntax_error: std::runtime_error {
  using std::runtime_error::runtime_error;
};


template<class Cont>
static void handle(Cont cont, std::ostream& err=std::cerr) try {
  return cont();
} catch(std::exception& e) {
  err << "error: " << e.what() << std::endl;;
};


int main(int, char**) {
  const auto parser = sexpr::parser() >>= drop(parser::eos);
  const auto parse = [&](const char* input) {
  };


  repl([&](const char* input) {
    return handle([&] {
      const auto s = rethrow_as<parse_error>([&] {
        return parser::run(parser, input);
      });

      std::clog << s << std::endl;

      const auto e = rethrow_as<syntax_error>([&]{
        return ast::check(s);
      });
    });
  });
  
  return 0;
}


