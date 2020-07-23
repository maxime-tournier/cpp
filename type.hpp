#ifndef TYPE_HPP
#define TYPE_HPP

#include "variant.hpp"
#include "symbol.hpp"
#include "fix.hpp"
#include "list.hpp"

namespace ast {
struct expr;
}

namespace type {

template<class T>
using ref = std::shared_ptr<const T>;

struct ctor;

struct kind_constant {
  symbol name;
  kind_constant(symbol name): name(name) { }

  static ref<kind_constant> make(symbol name);  
};

struct kind: variant<ref<kind_constant>, ctor> {
  using kind::variant::variant;

  kind operator>>=(kind other) const;
  bool operator==(kind other) const;

  std::string show() const;
};

extern const kind term, row;

struct ctor {
  kind from;
  kind to;

  bool operator==(ctor other) const {
    return std::tie(from, to) == std::tie(other.from, other.to);
  }
};



struct type_constant {
  symbol name;
  struct kind kind;

  type_constant(symbol name, struct kind kind): name(name), kind(kind) { }

  static ref<type_constant> make(symbol name, struct kind kind);
};




struct var {
  std::size_t depth;
  struct kind kind;
  var(std::size_t depth, struct kind kind): depth(depth), kind(kind) { }
};


template<class T>
struct App {
  // TODO ctor w/ kind check
  T ctor;
  T arg;
};

template<class T>
struct Mono: variant<ref<type_constant>, ref<var>, App<T>> {
  using Mono::variant::variant;

  template<class Func>
  friend auto map(const Mono& self, Func func) {
    using type = typename std::result_of<Func(T)>::type;
    using result_type = Mono<type>;
    return match(self,
                 [](ref<type_constant> self) -> result_type { return self; },
                 [](ref<var> self) -> result_type { return self; },
                 [&](App<T> self) -> result_type {
                   return App<type>{func(self.ctor), func(self.arg)};
                 });
  }
};



struct mono: fix<Mono, mono> {
  using mono::fix::fix;

  friend std::ostream& operator<<(std::ostream& out, mono self);
  struct kind kind() const;

  list<ref<var>> vars() const;

  mono operator>>=(mono to) const;
  mono operator()(mono arg) const;
};

using app = App<mono>;

extern const mono func, unit, boolean, integer, number, string;

template<class T>
struct Forall {
  ref<var> arg;
  T body;
};

template<class T>
struct Poly: variant<mono, Forall<T>> {
  using Poly::variant::variant;

  template<class Func>
  friend auto map(const Poly& self, Func func) {
    using type = typename std::result_of<Func(T)>::type;
    using result_type = Poly<type>;
    return match(self,
                 [](mono self) -> result_type { return self; },
                 [&](Forall<T> self) -> result_type {
                   return Forall<type>{self.arg, func(self.body)};
                 });
  }

};

struct poly: fix<Poly, poly> {
  using poly::fix::fix;

  mono body() const;
  list<ref<var>> bound() const;
};

using forall = Forall<poly>;

////////////////////////////////////////////////////////////////////////////////
struct context;
ref<context> make_context();

mono infer(ref<context> ctx, const ast::expr&);

}


#endif
