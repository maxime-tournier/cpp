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
  }

  void run(std::string code) {
    const int error = luaL_dostring(state, code.c_str());
    
    if(error) {
      std::string what = lua_tostring(state, -1);
      lua_pop(state, 1);
      throw std::runtime_error(what);
    }
  }
  
  ~environment() {
    lua_close(state);
  }
};


std::shared_ptr<environment> make_environment() {
  return std::make_shared<environment>();
}

template<class T>
static void compile(const T& self, std::ostream&) {
  throw std::runtime_error("unimplemented");
}

static void compile(const ast::expr& self, std::ostream& ss);

static void compile(const ast::lit& self, std::ostream& ss) {
  return match(self,
               [&](bool self) { ss << (self ? "true" : "false"); },
               [&](std::string self) { ss << '"' << self << '"'; },               
               [&](auto self) { ss << self; });               
}


static void compile(const ast::var& self, std::ostream& ss) {
  ss << self.name.repr;
}


static void compile(const ast::app& self, std::ostream& ss) {
  compile(self.func, ss);
  ss << "(";
  compile(self.arg, ss);
  ss << ")";
}


static const auto wrap = [](auto expr) {
  return [=](std::ostream& ss) {
    ss << "(function() ";
    expr(ss);
    ss << " end)()";
  };
};


static void compile(const ast::abs& self, std::ostream& ss) {
  ss << "(function(" << self.arg.name.repr << ") return ";
  compile(self.body, ss);
  ss << " end)";
}


static void compile(const ast::cond& self, std::ostream& ss) {
  return wrap([=](auto& ss) {
    ss << "if ";
    compile(self.pred, ss);
    ss << " then return ";
    compile(self.conseq, ss);
    ss << " else return ";
    compile(self.alt, ss);
    ss << " end";
  })(ss);
}

static void compile(const ast::let& self, std::ostream& ss) {
  return wrap([=](std::ostream& ss) {
    for(auto def: self.defs) {
      ss << "local " << def.name.repr << " = ";
      compile(def.value, ss);
      ss << '\n';
    }
    
    ss << "return ";
    compile(self.body, ss);

    ss << " end";
  })(ss);
}



static void compile(const ast::expr& self, std::ostream& ss) {
  return match(self, [&](auto self) {
    return compile(self, ss);
  });
}


std::string run(std::shared_ptr<environment> env, const ast::expr& self) {
  std::stringstream ss;
  ss << "return ";
  
  compile(self, std::clog);
  std::clog << std::endl;
  
  compile(self, ss);  
  env->run(ss.str());
  
  std::string result = luaL_tolstring(env->state, -1, nullptr);
  lua_pop(env->state, 1);
  return result;
}

}
