#ifndef SLIP_LUA_HPP
#define SLIP_LUA_HPP

namespace ast {
struct expr;
}

namespace lua {
struct environment;
std::shared_ptr<environment> make_environment();

std::string run(std::shared_ptr<environment> env, const ast::expr& self);
}


#endif
