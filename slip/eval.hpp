#ifndef SLIP_EVAL_HPP
#define SLIP_EVAL_HPP

#include "variant.hpp"
#include "shared.hpp"

#include <memory>

namespace ast {
struct expr;
}

namespace eval {

struct closure;
struct builtin;
struct record;

struct value: variant<bool, long, double, std::string, shared<record>, shared<closure>> {
  using value::variant::variant;

  std::string show() const;
};

struct environment;
std::shared_ptr<environment> make_environment();

value run(std::shared_ptr<environment> env, const ast::expr&);

} // namespace eval


#endif
