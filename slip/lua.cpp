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
  compile(self.arg, ss);
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

static void compile(const ast::abs& self, state& ss) {
  ss << "(function(" << self.arg.name.repr << ")";
  ss.with_indent([&] {
    ss.newline() << "return ";
    compile(self.body, ss);
  });
  ss.newline() << "end)";
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
        ss << "function " << def.name.repr << "(" << fun->arg.name.repr << ")";
        ss.with_indent([&] {
          ss.newline() << "return ";
          compile(fun->body, ss);
        });
        ss.newline() << "end";
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
