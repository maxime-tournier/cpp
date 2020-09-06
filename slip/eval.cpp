#include "eval.hpp"

#include <functional>
#include <stdexcept>

#include "ast.hpp"
#include "hamt.hpp"
#include "symbol.hpp"
#include "common.hpp"

namespace eval {

using table_type = hamt::map<symbol, value>;




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
  mutable symbol self = "__self__";
};

static symbol gensym(std::string prefix) {
  static std::size_t index = 0;
  symbol res = (prefix + std::to_string(index++)).c_str();
  return res;
}

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
    value f = func(env);
    const closure& c = f.template get<closure>();
    auto sub = c.env.set(c.arg, arg(env));

    // TODO only if needed
    sub = sub.set(c.self, std::move(f));
    return c.body(sub);
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

struct def {
  symbol name;
  monad<value> code;
};

static monad<value> compile(ast::let self) {
  const auto defs = map(self.defs, [](ast::def self) {
    return def{self.name, compile(self.value)};
  });

  const auto body = compile(self.body);
  
  return [=](auto env) {
    for(auto&& def: defs) {
      value v = def.code(env);
      if(auto c = v.template cast<closure>()) {
        c->self = def.name;
      }
      env = env.set(def.name, std::move(v));
    }

    return body(env);
  };
}


struct record {
  using attrs_type = hamt::map<symbol, value>;
  attrs_type attrs;
};

static monad<value> compile(ast::record self) {
  const auto defs = map(self.attrs, [](ast::def self) {
    return def{self.name, compile(self.value)};
  });

  return [defs](const auto& env) {
    record res;
    for(auto&& attr: defs) {
      res.attrs = std::move(res.attrs).set(attr.name, attr.code(env));
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
               [](bool self) -> std::string { return self ? "true" : "false"; },
               [](long self) { return std::to_string(self); },
               [](double self) { return std::to_string(self); },
               [](std::string self) { return quote(self); },
               [](record self) {
                 std::stringstream ss;
                 ss << "(record";

                 self.attrs.iter([&](symbol name, value val) {
                   ss << " (" << name << " " << val.show() << ")";
                 });

                 ss << ")";

                 // TODO abbreviate if too long
                 return ss.str();
               },
               [](closure self) { return std::string("#closure"); });
}


static monad<value> compile(ast::expr self) {
  return match(self, [](auto self) { return compile(self); });
}

static value wrap(value self) { return self; }

template<class Func>
static closure wrap(Func func) {
  const symbol arg = "__arg__";
  return {{}, arg, [=](const auto& env) {
    return wrap(func(env.get(arg)));
  }};
}


struct environment {
  table_type table;

  template<class T>
  void def(symbol name, T value) {
    table = table.set(name, wrap(value));
  }
};


value run(std::shared_ptr<environment> env, const ast::expr& self) {
  return compile(self)(env->table);
}


std::shared_ptr<environment> make_environment() {
  auto res = std::make_shared<environment>();

  res->def("eq", [=](value x) {
    return [=](value y) -> value {
      return x.get<long>() == y.get<long>();
    };
  });

  res->def("add", [=](value x) {
    return [=](value y) -> value {
      return x.get<long>() + y.get<long>();
    };
  });

  res->def("sub", [=](value x) {
    return [=](value y) -> value {
      return x.get<long>() - y.get<long>();
    };
  });

  res->def("mul", [=](value x) {
    return [=](value y) -> value {
      return x.get<long>() * y.get<long>();
    };
  });
  
  
  return res;
}


} // namespace eval


