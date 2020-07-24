#include "sexpr.hpp"
#include "ast.hpp"
#include "type.hpp"
#include "repl.hpp"

#include <iostream>

template<class Cont>
static void handle(Cont cont, std::ostream& err=std::cerr) try {
  return cont();
} catch(std::exception& e) {
  err << e.what() << std::endl;
};


int main(int, char**) {
  const auto parser = sexpr::parser() >>= drop(parser::eos);

  auto ctx = type::make_context();

  const auto history = ".slip";
  
  repl([&](const char* input) {
    return handle([&] {
      const auto s = parser::run(parser, input);
      const auto e = ast::check(s);
      const auto p = type::infer(ctx, e);
      std::cout << " :: " << p.show() << std::endl;
    });
  }, "> ", history);

  return 0;
}
