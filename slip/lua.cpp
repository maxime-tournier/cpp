#include "ast.hpp"
#include "type.hpp"

#include <sstream>
#include <iostream>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace lua {

struct environment {
  lua_State* state;

  environment(): state(luaL_newstate()) {
    luaL_openlibs(state);

    const int error = luaL_dofile(state, find("prelude.lua").c_str());
    check(error);
  }

  void check(int error) const {
    if(error) {
      std::string what = lua_tostring(state, -1);
      lua_pop(state, 1);
      throw std::runtime_error(what);
    }
  }
  

  std::string find(std::string filename) const {
    return SLIP_DIR + std::string("/") + filename;
  }
      
  void run(std::string code) {
    const int error = luaL_dostring(state, code.c_str());
    check(error);
  }
  
  ~environment() {
    lua_close(state);
  }
};


std::shared_ptr<environment> make_environment() {
  return std::make_shared<environment>();
}

struct ret;
struct def;
struct local;
struct call;
struct thunk;
struct cond;

struct var;
struct lit;
struct func;
struct table;
struct getattr;
struct nil;                     // TODO put in literals?

struct term: variant<ret, local, def, call, cond> {
  using term::variant::variant;
};

struct expr: variant<var, lit, call, func, thunk, table, getattr, nil> { 
  using expr::variant::variant;
};

struct lit: variant<bool, long, double, std::string> {
  using lit::variant::variant;
};

struct call {
  expr func;
  list<expr> args;
};

struct func {
  list<symbol> args;
  list<term> body;
};

struct thunk {
  list<term> body;
};

struct ret {
  expr value;
};

struct local {
  symbol name;
};

struct var {
  symbol name;
};

struct def {
  symbol name;
  expr value;
};

struct block {
  list<term> body;
};

struct cond {
  expr pred;
  list<term> conseq, alt; 
};

struct table {
  list<def> attrs;
};

struct getattr {
  expr arg;
  symbol name;
};

struct nil { };


class state {
  std::ostream& out;
  std::size_t indent = 0;

public:
  state(std::ostream& out): out(out) { }

  template<class T>
  state& operator<<(const T& value) {
    out << value;
    return *this;
  }

  state& newline() {
    (out << "\n" << std::string(2 * indent, ' '));;
    return *this;
  }
  
  state& operator<<(std::ostream& (*func)(std::ostream&)) {
    out << func;
    return *this;
  }

  template<class Cont>
  void with_indent(Cont cont) {
    const std::size_t old = indent++;
    cont();
    indent = old;
  }
  
};


struct context {
  hamt::array<type::mono> types;
};


template<class T>
static expr compile(context, T) {
  throw std::runtime_error("unimplemented compile: " +
                           std::string(typeid(T).name()));
}

static expr compile(context, ast::expr self);


static expr compile(context, ast::lit self) {
  return match(self, [](auto self) -> expr {
    return lit{self};
  });
}


static std::size_t arity(type::mono self) {
  // std::clog << "arity: " << self.show() << std::endl;
  return match(self,
               [=](type::app outer) {
                 // std::clog << "outer " << std::endl;
                 return match(outer.ctor,
                              [=](type::app inner) -> std::size_t {
                                // std::clog << "inner" << std::endl;
                                if(inner.ctor == type::func) {
                                  return 1 + arity(outer.arg);
                                }
                                std::clog << inner.ctor.show() << std::endl;
                                std::clog << inner.ctor.id() << std::endl;
                                std::clog << type::func.id() << std::endl; 
                                
                                return 0;
                              },
                              [](auto) { return 0; });
               },
               [](auto) { return 0; });
}


static expr compile(context ctx, ast::app self) {
  assert(ctx.types.find(self.func.id()));
  const std::size_t expected = arity(ctx.types.get(self.func.id()));
  const std::size_t given = size(self.args);
  // std::clog << "expected: " << expected << " given: " << given << std::endl;
  
  if(expected == given) {
    // TODO evaluation order?
    return call{compile(ctx, self.func), map(self.args, [=](ast::expr arg) {
      return compile(ctx, arg);
    })};
  } else if(expected > given) {
    // under-saturated call: closure
    const std::size_t rest = expected - given;

    // name func + given args
    const auto given_args = map(self.args, [=](ast::expr arg) {
      return def{symbol::unique("__tmp"), compile(ctx, arg)};
    });

    const def f = {symbol::unique("__func"), compile(ctx, self.func)};
    
    const list<def> defs = f %= given_args;
    
    // name remaining args
    list<symbol> rest_args;
    for(std::size_t i = 0; i < rest; ++i) {
      rest_args = symbol::unique("__arg") %= rest_args;
    }
    
    const list<expr> call_args =
      concat(map(given_args, [](def arg) -> expr { return var{arg.name}; }),
             map(rest_args, [](symbol arg) -> expr { return var{arg};}));
    

    const list<term> body = ret{call{var{f.name}, call_args}} %= list<term>();
    const list<term> init = ret{func{rest_args, body}} %= list<term>();
    
    return thunk{foldr(defs, init, [](term head, list<term> tail) {
      return head %= tail;
    })};
  } else {
    // over-saturated call: cannot happen yet, but will become possible once
    // proper tuple types are used for functions args.
    throw std::runtime_error("unimplemented: over-saturated calls");
  }

}

static expr compile(context ctx, ast::abs self) {
  const term body = ret{compile(ctx, self.body)};
  return func{map(self.args, [](ast::arg arg) {
    return arg.name();
  }), body %= list<term>() };
}

static expr compile(context, ast::var self) {
  return var{self.name};
}


static expr compile(context ctx, ast::cond self) {
  const expr pred = compile(ctx, self.pred);
  const term conseq = ret{compile(ctx, self.conseq)};
  const term alt = ret{compile(ctx, self.alt)};
  
  const term body = cond{pred,
    conseq %= list<term>(),
    alt %= list<term>()};
  
  return thunk{body %= list<term>()};
}


static expr compile(context ctx, ast::let self) {
  const list<term> decls = map(self.defs, [](ast::def self) -> term {
    return local{self.name};
  });

  const list<term> defs = map(self.defs, [=](ast::def self) -> term {
    return def{self.name, compile(ctx, self.value)};
  });

  const term body = ret{compile(ctx, self.body)};
  
  return thunk{concat(concat(decls, defs),
                      body %= list<term>())};
}


static expr compile(context ctx, ast::record self) {
  return table{map(self.attrs, [=](ast::def attr) {
    return def{attr.name, compile(ctx, attr.value)};
  })};
}

static expr compile(context ctx, ast::attr self) {
  return getattr{compile(ctx, self.arg), self.name};
}


static expr compile(ast::type self) {
  return nil{};
}


static expr compile(context ctx, ast::expr self) {
  return match(self, [=](auto self) {
    return compile(ctx, self);
  });
}


////////////////////////////////////////////////////////////////////////////////
static void format(expr self, state& ss);
static void format(term self, state& ss);

template<class T>
static void format(T self, state& ss) {
  throw std::runtime_error("unimplemented format: " +
                           std::string(typeid(T).name()));
}

static void format(lit self, state& ss) {
  return match(self,
               [&](bool self) { ss << (self ? "true" : "false"); },
               [&](std::string self) { ss << '"' << self << '"'; },   
               [&](auto self) { ss << self; });               
}

static void format(var self, state& ss) {
  ss << self.name.repr;
}


static void format(ret self, state& ss) {
  format(self.value, ss << "return ");
}

static void format(local self, state& ss) {
  ss << "local " << self.name;
}

static void format(nil self, state& ss) {
  ss << "nil";
}


static void format(def self, state& ss) {
  ss << self.name << " = ";
  format(self.value, ss);
}



static void format(call self, state& ss) {
  format(self.func, ss);
  ss << "(";
  unsigned sep = 0;
  for(auto arg: self.args) {
    if(sep++) {
      ss << ", ";
    }
    format(arg, ss);
  }
  ss << ")";
}


static void format(thunk self, state& ss) {
  ss << "(function()";
  ss.with_indent([&] {
    for(term t: self.body) {
      format(t, ss.newline());
    }
  });
  ss.newline() << "end)()";
}




static void format(table self, state& ss) {
  ss << "{";

  int sep = 0;
  for(auto def: self.attrs) {
    if(sep++) {
      ss << ", ";
    }
    
    ss << def.name.repr << " = ";
    format(def.value, ss);
  }
  ss << "}";
}


static void format(getattr self, state& ss) {
  ss << "(";
  format(self.arg, ss);
  ss << ").";
  ss << self.name.repr;
}


static void format(func self, state& ss) {
  ss << "(function(";
  
  unsigned sep = 0;
  for(auto arg: self.args) {
    if(sep++) {
      ss << ", ";
    }
    ss << arg.repr;
  }

  ss << ")";
  ss.with_indent([&] {
    for(term t: self.body) {
      format(t, ss.newline());
    }
  });
  ss.newline() << "end)";
}

static void format(cond self, state& ss) {
  format(self.pred, ss << "if ");

  ss.newline() << "then";
  ss.with_indent([&] {
    for(term t: self.conseq) {
      format(t, ss.newline());
    }
  });
  ss.newline() << "else";
  ss.with_indent([&] {
    for(term t: self.alt) {
      format(t, ss.newline());
    }
  });
  ss.newline() << "end";
}



////////////////////////////////////////////////////////////////////////////////

static void format(expr self, state& ss) {
  return match(self, [&](auto self) {
    return format(self, ss);
  });
}

static void format(term self, state& ss) {
  return match(self, [&](auto self) {
    return format(self, ss);
  });
}

////////////////////////////////////////////////////////////////////////////////
// TODO optimization



std::string run(std::shared_ptr<environment> env,
                const ast::expr& self,
                const hamt::array<type::mono>& types) {
  std::stringstream buffer;
  state ss(buffer);

  context ctx;
  ctx.types = types;
  
  const term code = ret{compile(ctx, self)};
  format(code, ss);

  // debug
  std::clog << buffer.str() << std::endl;

  env->run(buffer.str());
  
  const std::string result = luaL_tolstring(env->state, -1, nullptr);
  lua_pop(env->state, 1);
  return result;
}

} // namespace lua
