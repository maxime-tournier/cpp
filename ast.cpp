#include "ast.hpp"
#include "sexpr.hpp"
#include "either.hpp"
#include "repl.hpp"

namespace ast {

expr check(sexpr e) {
  return match(e,
               [](auto self) -> expr {
                 return lit{self};
               },
               [](symbol self) -> expr {
                 return var{self};
               },
               [](sexpr::list self) -> expr {
                 if(self) {
                   throw std::runtime_error("empty list in application");
                 }

                 match(self->head,
                       [](symbol first) {

                       },
                       [](sexpr func) {
                         
                       });
                 
                 throw std::runtime_error("not implemented");
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


