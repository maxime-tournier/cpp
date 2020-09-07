#include "sexpr.hpp"
#include "ast.hpp"
#include "type.hpp"
#include "eval.hpp"

#include "repl.hpp"
#include "lua.hpp"

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
  // auto env = eval::make_environment();
  auto env = lua::make_environment();
  const auto history = ".slip";

  repl([&](const char* input) {
    return handle([&] {
      const auto s = parser::run(parser, input);
      const auto e = ast::check(s);
      const auto p = type::infer(ctx, e);
      const auto v = lua::run(env, e);
      std::cout << " :: " << p.show() << " = " << v << std::endl;      
    });
  }, "> ", history);

  return 0;
}

int main_load(std::string filename) try {
  auto ctx = type::make_context();
  // auto env = eval::make_environment();
  auto env = lua::make_environment();
  
  if(std::ifstream ifs{filename}) {
    for(auto s: parser::run(program(), ifs)) {
      const auto e = ast::check(s);
      const auto p = infer(ctx, e);
      const auto v = lua::run(env, e);
      std::cout << " :: " << p.show() << " = " << v << std::endl;
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
