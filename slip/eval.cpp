#include "eval.hpp"

#include <functional>
#include <stdexcept>

#include "ast.hpp"
#include "hamt.hpp"
#include "symbol.hpp"
#include "common.hpp"

namespace eval {

using table_type = hamt::map<symbol, value>;

struct environment {
  table_type table;
};

std::shared_ptr<environment> make_environment() {
  return std::make_shared<environment>();
}


template<class T>
using monad = std::function<T(const table_type&)>;

static monad<value> compile(const ast::expr& self);

template<class T>
static monad<value> compile(const T& self) {
  throw std::runtime_error("unimplemented compile: " + std::string(typeid(T).name()));
}

static monad<value> compile(const ast::lit& self) {
  return match(self, [](auto self) -> monad<value> {
    return [=](const auto&) { return self; };
  });
}

struct closure {
  table_type table;
  symbol arg;
  monad<value> body;
};

static monad<value> compile(const ast::abs& self) {
  return [body = compile(self.body),
          arg = self.arg.name](const auto& env) -> value {
    return shared<closure>(env, arg, body);
  };
}


static monad<value> compile(const ast::app& self) {
  const auto func = compile(self.func);
  const auto arg = compile(self.arg);  
  
  return [=](const auto& env) {
    const value f = func(env);
    const auto& c = f.template get<shared<closure>>();
    return c->body(env.set(c->arg, arg(env)));
  };
}


static monad<value> compile(const ast::var& self) {
  return [name=self.name](const auto& env) {
    return env.get(name);
  };
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


static monad<value> compile(const ast::expr& self) {
  return match(self, [](const auto& self) { return compile(self); });
}

value run(std::shared_ptr<environment> env, const ast::expr& self) {
  return compile(self)(env->table);

}

} // namespace eval


