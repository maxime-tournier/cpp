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
  symbol attr;
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


namespace alt {

template<class T>
static expr compile(T) {
  throw std::runtime_error("unimplemented");
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


static void format(expr self, state& ss);

template<class T>
static void format(T self, state& ss) {
  throw std::runtime_error("unimplemented");
}

static void format(lit self, state& ss) {
  return match(self,
               [&](bool self) { ss << (self ? "true" : "false"); },
               [&](std::string self) { ss << '"' << self << '"'; },   
               [&](auto self) { ss << self; });               
}



} // namespace alt


template<class T>
static void compile(const T& self, state&) {
  throw std::runtime_error("unimplemented");
}



static void compile(const ast::expr& self, state& ss);

static void compile(const ast::lit& self, state& ss) {
  return match(self,
               [&](bool self) { ss << (self ? "true" : "false"); },
               [&](std::string self) { ss << '"' << self << '"'; },   
               [&](auto self) { ss << self; });               
}


static void compile(const ast::var& self, state& ss) {
  ss << self.name.repr;
}


static void compile(const ast::app& self, state& ss) {
  compile(self.func, ss);
  ss << "(";
  unsigned sep = 0;
  for(auto arg: self.args) {
    if(sep++) {
      ss << ", ";
    }
    compile(arg, ss);
  }
  ss << ")";
}


static const auto thunk = [](auto expr) {
  return [=](state& ss) {
    ss << "(function()";
    ss.with_indent([&] {
      expr(ss.newline());
    });
    ss.newline() << "end)()";
  };
};


static void compile(const ast::record& self, state& ss) {
  ss << "{";

  int sep = 0;
  for(ast::def def: self.attrs) {
    if(sep++) {
      ss << ", ";
    }
    
    ss << def.name.repr << " = ";
    compile(def.value, ss);
  }
  ss << "}";
}

static void compile(const ast::attr& self, state& ss) {
  ss << "(";
  compile(self.arg, ss);
  ss << ").";
  ss << self.name.repr;
}


static void compile(const ast::abs& self, state& ss, const char* name) {
  ss << "function";

  if(name) {
    ss << " " << name;
  }
  
  ss << "(";
  
  unsigned sep = 0;
  for(auto arg: self.args) {
    if(sep++) {
      ss << ", ";
    }
    ss << arg.name.repr;
  }

  ss << ")";
  ss.with_indent([&] {
    ss.newline() << "return ";
    compile(self.body, ss);
  });
  ss.newline() << "end";
}

static void compile(const ast::abs& self, state& ss) {
  ss << "(";
  compile(self, ss, nullptr);
  ss << ")";
}


static void compile(const ast::cond& self, state& ss) {
  return thunk([=](auto& ss) {
    ss << "if ";
    compile(self.pred, ss);
    ss.newline() << "then return ";
    compile(self.conseq, ss);
    ss.newline() << "else return ";
    compile(self.alt, ss);
    ss.newline() << "end";
  })(ss);
}

static void compile(const ast::let& self, state& ss) {
  return thunk([=](state& ss) {
    int sep = 0;
    for(auto def: self.defs) {
      if(sep++) {
        ss.newline();
      }
      if(auto fun = def.value.cast<ast::abs>()) {
        compile(*fun, ss, def.name.repr);
      } else {
        ss << "local " << def.name.repr << " = ";
        compile(def.value, ss);
      }
    }
    
    ss.newline() << "return ";
    compile(self.body, ss);
  })(ss);
}



static void compile(const ast::expr& self, state& ss) {
  return match(self, [&](auto self) {
    return compile(self, ss);
  });
}


std::string run(std::shared_ptr<environment> env, const ast::expr& self) {
  std::stringstream buffer;
  state ss(buffer);
  
  ss << "return ";
  
  compile(self, ss);
  ss << std::flush;

  std::clog << buffer.str() << std::endl;

  env->run(buffer.str());
  
  std::string result = luaL_tolstring(env->state, -1, nullptr);
  lua_pop(env->state, 1);
  return result;
}

}
