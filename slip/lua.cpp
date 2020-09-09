#include "ast.hpp"

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

struct term: variant<ret, local, def, call, cond> {
  using term::variant::variant;
};

struct expr: variant<var, lit, call, func, thunk, table, getattr> {
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



template<class T>
static expr compile(T) {
  throw std::runtime_error("unimplemented compile: " +
                           std::string(typeid(T).name()));
}

static expr compile(ast::expr self);


static expr compile(ast::lit self) {
  return match(self, [](auto self) -> expr {
    return lit{self};
  });
}


static expr compile(ast::app self) {
  return call{compile(self.func), map(self.args, [](auto arg) {
    return compile(arg);
  })};
}

static expr compile(ast::abs self) {
  const term body = ret{compile(self.body)};
  return func{map(self.args, [](auto arg) {
    return arg.name;
  }), body %= list<term>() };
}

static expr compile(ast::var self) {
  return var{self.name};
}


static expr compile(ast::cond self) {
  const expr pred = compile(self.pred);
  const term conseq = ret{compile(self.conseq)};
  const term alt = ret{compile(self.alt)};
  
  const term body = cond{pred,
    conseq %= list<term>(),
    alt %= list<term>()};
  
  return thunk{body %= list<term>()};
}


static expr compile(ast::let self) {
  const list<term> decls = map(self.defs, [](ast::def self) -> term {
    return local{self.name};
  });

  const list<term> defs = map(self.defs, [](ast::def self) -> term {
    return def{self.name, compile(self.value)};
  });

  const term body = ret{compile(self.body)};
  
  return thunk{concat(concat(decls, defs),
                      body %= list<term>())};
}


static expr compile(ast::record self) {
  return table{map(self.attrs, [](auto attr) {
    return def{attr.name, compile(attr.value)};
  })};
}

static expr compile(ast::attr self) {
  return getattr{compile(self.arg), self.name};
}


static expr compile(ast::expr self) {
  return match(self, [](auto self) {
    return compile(self);
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
  ss << "function(";
  
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
  ss.newline() << "end";
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



std::string run(std::shared_ptr<environment> env, const ast::expr& self) {
  std::stringstream buffer;
  state ss(buffer);

  const term code = ret{compile(self)};
  format(code, ss);

  std::clog << buffer.str() << std::endl;

  env->run(buffer.str());
  
  std::string result = luaL_tolstring(env->state, -1, nullptr);
  lua_pop(env->state, 1);
  return result;
}

} // namespace lua
