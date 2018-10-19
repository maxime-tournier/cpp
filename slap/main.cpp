#include <map>
#include <iostream>
#include <sstream>
#include <fstream>

#include <readline/readline.h>
#include <readline/history.h>

#include "sexpr.hpp"
#include "ast.hpp"
#include "eval.hpp"

#include "parse.hpp"

#include "type.hpp"
#include "package.hpp"

#include "argparse.hpp"

const bool debug = false;


struct history {
  const std::string filename;
  history(const std::string& filename="/tmp/slap.history")
    : filename(filename) {
    read_history(filename.c_str());
  }
    
  ~history() {
    write_history(filename.c_str());
  }
};


template<class F>
static void read_loop(const F& f) {
  const history lock;
  while(const char* line = readline("> ")) {
    if(!*line) continue;
    
    add_history(line);
    std::stringstream ss(line);
	
    f(ss);
  }
};


static void print_error(const std::exception& e, std::size_t level=0) {
  const std::string prefix(level, '.');
    
  std::cerr << prefix << e.what() << std::endl;
    
  try {
    std::rethrow_if_nested(e);
  } catch(std::exception& e) {
    print_error(e, level + 1);
  }
  
}



int main(int argc, const char** argv) {

  using namespace argparse;
  const auto parser = argparse::parser
    (flag("debug")
     flag("ast") |
     argument<std::string>("filename"))
    ;

  const auto options = parser.parse(argc, argv);

  const bool show_ast = 
  
  // parser::debug::stream = &std::clog;
  package pkg = package::core();
  
  static const auto handler =
    [&](std::istream& in) {
    try {
      ast::expr::iter(in, [&](ast::expr e) {
          if(options.flag("ast", false)) {
            std::cout << "ast: " << e << std::endl;
          }
          pkg.exec(e, [&](type::poly p, eval::value v) {
            // TODO: cleanup variables with depth greater than current in
            // substitution
            if(auto self = e.get<ast::var>()) {
              std::cout << self->name;
            }

            std::cout << " : " << p << std::flush
                      << " = " << v << std::endl;
          });
        });
        return true;
      } catch(sexpr::error& e) {
        std::cerr << "parse error: " << e.what() << std::endl;
      } catch(ast::error& e) {
        std::cerr << "syntax error: " << e.what() << std::endl;
      } catch(type::error& e) {
        std::cerr << "type error: " << std::endl;
        print_error(e);
      } catch(kind::error& e) {
        std::cerr << "kind error: " << e.what() << std::endl;
      } catch(std::runtime_error& e) {
        std::cerr << "runtime error: " << e.what() << std::endl;
      }
      return false;
    };

  
  if(auto filename = options.get<std::string>("filename")) {
    if(auto ifs = std::ifstream(filename->c_str())) {
      return handler(ifs) ? 0 : 1;
    } else {
      std::cerr << "io error: " << "cannot read " << filename << std::endl;
      return 1;
    }
  } else {
    read_loop(handler);
  }
  
  return 0;
}
