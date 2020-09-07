#include "ast.hpp"

#include <sstream>

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
static void compile(const T& self, std::stringstream&) {
  throw std::runtime_error("unimplemented");
}

static void compile(const ast::lit& self, std::stringstream& ss) {
  return match(self,
               [&](bool self) { ss << (self ? "true" : "false"); },
               [&](std::string self) { ss << '"' << self << '"'; },               
               [&](auto self) { ss << self; });               
}



static void compile(const ast::expr& self, std::stringstream& ss) {
  return match(self, [&](auto self) {
    return compile(self, ss);
  });
}


std::string run(std::shared_ptr<environment> env, const ast::expr& self) {
  std::stringstream ss;
  ss << "return ";
  
  compile(self, ss);
  env->run(ss.str());
  
  std::string result = luaL_tolstring(env->state, -1, nullptr);
  lua_pop(env->state, 1);
  return result;
}

}
