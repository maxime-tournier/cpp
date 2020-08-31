#include "eval.hpp"

#include <functional>
#include <stdexcept>

#include "ast.hpp"
#include "hamt.hpp"
#include "symbol.hpp"
#include "common.hpp"

namespace eval {

struct environment {
  hamt::map<symbol, value> defs;
};

std::shared_ptr<environment> make_environment() {
  return std::make_shared<environment>();
}


template<class T>
using monad = std::function<value(std::shared_ptr<environment>)>;


template<class T>
static monad<value> eval(const T& self) {
  std::clog << "derp" << std::endl;
  throw std::runtime_error("unimplemented eval: " + std::string(typeid(T).name()));
}


std::string value::show() const {
  return match(*this,
               [](bool value) -> std::string { return value ? "true" : "false"; },
               [](long value) { return std::to_string(value); },
               [](double value) { return std::to_string(value); },
               [](std::string value) { return quote(value); },
               [](shared<record> value) { return std::string("#record"); },
               [](shared<closure> value) { return std::string("#closure"); });
}


value run(std::shared_ptr<environment> env, const ast::expr& self) {
  return match(self,
               [&](const auto& self) -> value {
                 return eval(self)(env);
               });

}

} // namespace eval


