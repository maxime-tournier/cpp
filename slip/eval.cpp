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

static monad<value> compile(ast::expr self);

template<class T>
static monad<value> compile(T self) {
  throw std::runtime_error("unimplemented compile: " + std::string(typeid(T).name()));
}

static monad<value> compile(ast::lit self) {
  return match(self, [](auto self) -> monad<value> {
    return [=](const auto&) { return self; };
  });
}

struct closure {
  table_type env;
  symbol arg;
  monad<value> body;
};

static monad<value> compile(ast::abs self) {
  return [body = compile(self.body),
          arg = self.arg.name](const auto& env) {
    return closure{env, arg, body};
  };
}


static monad<value> compile(ast::app self) {
  const auto func = compile(self.func);
  const auto arg = compile(self.arg);  
  
  return [=](const auto& env) {
    const value f = func(env);
    const closure& c = f.template get<closure>();
    return c.body(c.env.set(c.arg, arg(env)));
  };
}


static monad<value> compile(ast::var self) {
  return [name=self.name](const auto& env) {
    return env.get(name);
  };
}


static monad<value> compile(ast::cond self) {
  return [pred=compile(self.pred),
          conseq=compile(self.conseq),
          alt=compile(self.alt)](const auto& env) {
    if(pred(env).template get<bool>()) {
      return conseq(env);
    } else {
      return alt(env);
    }
  };
}


struct record {
  using attrs_type = hamt::map<symbol, value>;
  attrs_type attrs;
};

static monad<value> compile(ast::record self) {
  struct attr {
    symbol name;
    monad<value> def;
  };
  
  const auto defs = map(self.attrs, [](ast::def self) {
    return attr{self.name, compile(self.value)};
  });

  return [defs](const auto& env) {
    record res;
    for(auto&& attr: defs) {
      res.attrs = std::move(res.attrs).set(attr.name, attr.def(env));
    }
    return res;
  };
}


static monad<value> compile(ast::attr self) {
  auto arg = compile(self.arg);
  auto name = self.name;
  return [=](const auto& env) {
    return arg(env).template get<record>().attrs.get(name);
  };
}


std::string value::show() const {
  return match(*this,
               [](bool value) -> std::string { return value ? "true" : "false"; },
               [](long value) { return std::to_string(value); },
               [](double value) { return std::to_string(value); },
               [](std::string value) { return quote(value); },
               [](record value) { return std::string("#record"); },
               [](closure value) { return std::string("#closure"); });
}


static monad<value> compile(ast::expr self) {
  return match(self, [](auto self) { return compile(self); });
}

value run(std::shared_ptr<environment> env, const ast::expr& self) {
  return compile(self)(env->table);
}

} // namespace eval


