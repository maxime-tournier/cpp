#include "sexpr.hpp"
#include "ast.hpp"
#include "type.hpp"

#include "repl.hpp"
#include "lua.hpp"

#include <iostream>
#include <fstream>

template<class Cont>
static void with_show_errors(Cont cont, std::ostream& err=std::cerr) try {
  return cont();
} catch(std::exception& e) {
  err << e.what() << std::endl;
  throw;
};


static auto program() {
  using namespace parser;
  const auto space = skip(pred(std::isspace));
  return parser::list(sexpr::parse(), space) >>= drop(space >> eos);
}


static const auto process = [](auto ctx, auto env) {
  return [=](sexpr s) {
      const auto e = ast::check(s);
      hamt::array<type::mono> types;
      const auto p = type::infer(ctx, e, &types);
      std::cout << " :: " << p.show();
      if(env) {
        const auto v = lua::run(env, e, types);
        std::cout << " = " << v;
      }
      
      std::cout << '\n';
  };
};


static const auto with_repl = [](auto process) {
  const auto parser = sexpr::parse() >>= drop(parser::eos);
  const auto history = ".slip";

  repl([&](const char* input) {
    try {
      return with_show_errors([&] {
        const auto s = parser::run(parser, input);
        process(s);
      });
    } catch(std::runtime_error&) {
      
    }
  }, "> ", history);

  return 0;
};


static const auto with_load = [](std::string filename, auto process) -> int {
  try {
    with_show_errors([&] {
      if(std::ifstream ifs{filename}) {
        for(auto s: parser::run(program(), ifs)) {
          process(s);
        }

        return;
      }

      throw std::runtime_error("cannot read file: " + filename);
    });

    return 0;
  } catch(std::runtime_error& e) {
    return 1;
  }
};
  
int main(int argc, char** argv) {
  const auto p = process(type::make_context(),
                         lua::make_environment());
  if(argc > 1) {
    return with_load(argv[1], p);
  } else {
    return with_repl(p);
  }
}
