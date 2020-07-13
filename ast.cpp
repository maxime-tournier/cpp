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
                 throw std::runtime_error("not implemented");
               });
}

}


int main(int, char**) {
  const auto parser = sexpr::parser();
  
  repl([&](const char* input) {
    const auto e = run(parser, input);
    std::clog << e << std::endl;
  });
  return 0;
}


