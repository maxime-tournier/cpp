#include "ast.hpp"
#include "sexpr.hpp"
#include "either.hpp"
#include "repl.hpp"

#include <map>
#include <functional>

namespace ast {

struct syntax_error: std::runtime_error {
  syntax_error(std::string what): std::runtime_error("syntax error: " + what) { }
};

////////////////////////////////////////////////////////////////////////////////
// sexpr list parser monad
template<class T>
struct success {
  using value_type = T;
  value_type value;
  sexpr::list rest;
};

template<class T>
using result = either<std::string, success<T>>;

template<class T>
static result<T> make_success(T value, sexpr::list rest) {
  return success<T>{value, rest};
}

template<class Parser>
using value = typename std::result_of<Parser(sexpr::list)>::type::value_type::value_type;

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
  return [=](sexpr::list rest) -> result<T> {
    return match(self,
                 [=](T expected) -> result<T> {
                   return make_success(expected, rest);
                 },
                 [=](auto) -> result<T> {
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



template<class Parser, class Func, class=value<Parser>>
static auto operator>>=(Parser parser, Func func) { return bind(parser, func); }

template<class LHS, class RHS, class=value<LHS>, class=value<RHS>>
static auto operator>>(LHS lhs, RHS rhs) {
  return lhs >>= [rhs](auto) {
    return rhs;
  };
}

template<class Parser, class Func, class=value<Parser>>
static auto operator|=(Parser parser, Func func) { return map(parser, func); }

static const auto coproduct = [](auto lhs, auto rhs) {
  return [=](sexpr::list args) {
    if(auto res = lhs(args)) {
      return res;
    } else return rhs(args);
  };
};

template<class LHS, class RHS, class=value<LHS>, class=value<RHS>>
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
    throw syntax_error(result.left());
  }
};

template<class T>
static auto fail(std::string what) {
  return [=](sexpr::list) -> result<T> {
    return what;
  };
};

static auto check_list = [](auto func) {
  using func_type = decltype(func);
  using result_type = typename std::result_of<func_type(sexpr)>::type;
  using value_type = value<result_type>;
  using list_type = list<value_type>;
  
  return fix<list_type>([=](auto self) {
    return (empty >> pure(list_type{}))
      | ((pop >>= func) >>= [=](auto first) {
        return self >>= [=](auto rest) {
          return pure(first %= rest);
        };
      });
  });
 };


////////////////////////////////////////////////////////////////////////////////

// check function arguments
static const auto check_args = check_list([](sexpr item) {
  return expect<symbol>(item) |= [](symbol name) {
    return var{name};
  };
});

// check function definition
static const auto check_abs =
  ((pop >>= expect<sexpr::list>) >>= [](sexpr::list args) {
    return pop >>= [=](sexpr body) {
      return empty >> [=](sexpr::list) -> result<expr> {
        return (check_args |= [=](list<var> args) -> expr {
          return foldr(args, check(body), [](var arg, expr body) -> expr {
            return abs{arg, body};
          });
        })(args);
      };
    };
  }) | fail<expr>("(fn (`sym`...) `expr`)");


// conditionals
static const auto check_cond = (pop >>= [](sexpr pred) {
  return pop >>= [=](sexpr conseq) {
    return pop >>= [=](sexpr alt) {
      const expr res = cond{check(pred), check(conseq), check(alt)};
      return empty >> pure(res);
    };
  };
 }) | fail<expr>("(if `expr` `expr` `expr`)");


// definition
static const auto check_def = ((pop >>= expect<symbol>) >>= [](symbol name) {
  return pop >>= [=](sexpr value) {
    return empty >> pure(def{name, check(value)});
  };
 }) | fail<def>("(`sym` `expr`) expected for definition");


static const auto check_defs = check_list([](sexpr item) {
  // TODO prevent redefinitions
  return (expect<sexpr::list>(item) |= [](sexpr::list def) {
    return run(check_def, def);
  }) | fail<def>("((`sym` `expr`)...) expected for definitions");
});


// let
static const auto check_let = ((pop >>= expect<sexpr::list>) >>= [](sexpr::list defs) {
  return pop >>= [=](sexpr body) {
    const expr res = let{run(check_defs, defs), check(body)};
    return empty >> pure(res);
  };
 }) | fail<expr>("(let ((`sym` `expr`)...) `expr`)");


// open
static const auto check_open = pop >>= [](sexpr arg) {
  const expr res = open{check(arg)};
  return empty >> pure(res);
};


// record
static const auto check_record = check_defs >>= [](list<ast::def> defs) {
  const expr res = record{defs};
  return pure(res);
};

// special forms
using special_type = std::function<result<expr>(sexpr::list)>;

static const std::map<symbol, special_type> special = {
    {"fn", check_abs},
    {"if", check_cond},
    {"let", check_let},
    {"open", check_open},
    {"record", check_record},
};

static expr check_app(sexpr func, sexpr::list args) {
  return foldl(check(func), args, [](expr func, sexpr arg) -> expr {
    return app{func, check(arg)};
  });
}

expr check(const sexpr& e) {
  return match(
      e,
      [](auto self) -> expr { return lit{self}; },
      [](attrib self) -> expr {
        return attr{check(self.arg), self.name};
      },
      [](symbol self) -> expr {
        static const std::map<symbol, expr> special = {
            {"true", lit{true}},
            {"false", lit{false}},
        };
        const auto it = special.find(self);
        if(it != special.end()) return it->second;
        
        return var{self};
      },
      [](sexpr::list self) -> expr {
        if(!self) {
          throw syntax_error("empty list in application");
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


// template<class Cont>
// static void handle(Cont cont, std::ostream& err=std::cerr) try {
//   return cont();
// } catch(std::exception& e) {
//   err << e.what() << std::endl;
// };


// int main(int, char**) {
//   const auto parser = sexpr::parser() >>= drop(parser::eos);
  
//   repl([&](const char* input) {
//     return handle([&] {
//       const auto s = parser::run(parser, input);
//       std::clog << s << std::endl;

//       const auto e = ast::check(s);
//     });
//   });
  
//   return 0;
// }


