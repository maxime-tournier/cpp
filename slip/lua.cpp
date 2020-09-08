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
  std::ostream& operator<<(const T& value) {
    return (out << value);
  }

  std::ostream& newline() {
    return (out << "\n" << std::string(2 * indent, ' '));
  }
  
  std::ostream& operator<<(std::ostream& (*func)(std::ostream&)) {
    return out << func;
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


static const auto wrap = [](auto expr) {
  return [=](state& ss) {
    ss << "(function()\n";
    ss.with_indent([&] {
      expr(ss);
    });
    ss << "\nend)()";
  };
};


static void compile(const ast::abs& self, state& ss) {
  ss << "(function(" << self.arg.name.repr << ")";
  ss.with_indent([&] {
    ss.newline() << "return ";
    compile(self.body, ss);
  });
  ss.newline() << "end)";
}


static void compile(const ast::cond& self, state& ss) {
  return wrap([=](auto& ss) {
    ss << "if ";
    compile(self.pred, ss);
    ss << "\nthen return ";
    compile(self.conseq, ss);
    ss << "\nelse return ";
    compile(self.alt, ss);
    ss << "\nend";
  })(ss);
}

static void compile(const ast::let& self, state& ss) {
  return wrap([=](state& ss) {
    for(auto def: self.defs) {
      if(auto fun = def.value.cast<ast::abs>()) {
        ss << "function " << def.name.repr << "(" << fun->arg.name.repr << ")"
           << "\n return ";
        compile(fun->body, ss);
        ss << "\nend";
      } else {
        ss << "local " << def.name.repr << " = ";
        compile(def.value, ss);
      }
      ss << '\n';
    }
    
    ss << "return ";
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
