#ifndef INFER_HPP
#define INFER_HPP

#include "variant.hpp"
#include "ast.hpp"

namespace infer {

template<class T>
using ref = std::shared_ptr<const T>;

struct ctor;

struct kind_constant {
  symbol name;
  kind_constant(symbol name): name(name) { }
};

struct kind: variant<ref<kind_constant>, ctor> {
  using kind::variant::variant;
};

extern const kind term, row;

struct ctor {
  kind from;
  kind to;

  bool operator==(ctor other) const {
    return std::tie(from, to) == std::tie(other.from, other.to);
  }
};


static kind operator>>=(kind lhs, kind rhs) {
  return ctor{lhs, rhs};
}

static bool operator==(kind lhs, kind rhs) {
  if(lhs.type() != rhs.type()) {
    return false;
  }
  return match(
      lhs,
      [&](ctor lhs) {
        return std::tie(lhs.from, lhs.to) ==
               std::tie(rhs.get<ctor>().from, rhs.get<ctor>().to);
      },
      [](auto) { return true; });
}


struct type_constant {
  symbol name;
  struct kind kind;
  type_constant(symbol name, struct kind kind): name(name), kind(kind) { }
};


struct var {
  std::size_t depth;
  struct kind kind;
  var(std::size_t depth, struct kind kind): depth(depth), kind(kind) { }
};

struct app;

struct mono: variant<ref<type_constant>, ref<var>, app> {
  using mono::variant::variant;

  struct kind kind() const;

  friend mono operator>>=(mono lhs, mono rhs);
};

extern const mono func;

struct app {
  mono ctor;
  mono arg;

  app(mono ctor, mono arg);
  struct kind kind() const;
};


}


#endif
