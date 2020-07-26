#include "sexpr.hpp"
#include "ast.hpp"
#include "type.hpp"
#include "repl.hpp"

#include <iostream>
#include <fstream>

template<class Cont>
static void handle(Cont cont, std::ostream& err=std::cerr) try {
  return cont();
} catch(std::exception& e) {
  err << e.what() << std::endl;
};


static auto program() {
  using namespace parser;
  const auto space = skip(pred(std::isspace));
  return parser::list(sexpr::parser(), space) >>= drop(space >> eos);
}


int main_repl() {
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

int main_load(std::string filename) try {
  auto ctx = type::make_context();
  
  if(std::ifstream ifs{filename}) {
    for(auto s: parser::run(program(), ifs)) {
      const auto e = ast::check(s);
      const auto p = type::infer(ctx, e);
      std::cout << " :: " << p.show() << std::endl;
    }
    
    return 0;
  }
  
  throw std::runtime_error("cannot read file: " + filename);
} catch(std::runtime_error& e) {
  std::cerr << e.what() << std::endl;
  return 1;
}

int main(int argc, char** argv) {
  if(argc > 1) {
    return main_load(argv[1]);
  } else {
    return main_repl();
  }
}
