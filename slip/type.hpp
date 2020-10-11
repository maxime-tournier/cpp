#ifndef TYPE_HPP
#define TYPE_HPP

#include "variant.hpp"
#include "symbol.hpp"
#include "fix.hpp"
#include "list.hpp"
#include "shared.hpp"

#include <map>
#include <functional>

namespace ast {
struct expr;
}

namespace type {

struct ctor;

struct kind_constant_info {
  symbol name;
};

using kind_constant = shared<kind_constant_info>;

struct kind: variant<kind_constant, ctor> {
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

struct poly;
struct type_constant_info;
using type_constant = shared<type_constant_info>;

struct type_constant_info {
  symbol name;
  struct kind kind;
  bool flip;

  using open_type = std::function<poly(type_constant self)>;
  open_type open;
  
  type_constant_info(symbol name, struct kind kind=term,
                     bool flip=false, open_type open={}):
    name(name), kind(kind), flip(flip), open(open) {}

};



struct var_info {
  std::size_t depth;
  struct kind kind;
};

using var = shared<var_info>;


template<class T>
struct App {
  // TODO ctor w/ kind check
  T ctor;
  T arg;
};

template<class T>
struct Type: variant<type_constant, var, App<T>> {
  using Type::variant::variant;

  template<class Func>
  friend auto map(const Type& self, Func func) {
    using type = typename std::result_of<Func(T)>::type;
    using result_type = Type<type>;
    return match(self,
                 [](type_constant self) -> result_type { return self; },
                 [](var self) -> result_type { return self; },
                 [&](App<T> self) -> result_type {
                   return App<type>{func(self.ctor), func(self.arg)};
                 });
  }
};



// TODO use hamt::map instead?
using repr_type = std::map<var, std::string>;

struct mono: fix<Type, mono> {
  using mono::fix::fix;
  
  std::string show(repr_type={}) const;
  struct kind kind() const;

  list<var> vars() const;

  mono operator>>=(mono to) const;
  mono operator()(mono arg) const;
};



using app = App<mono>;

extern const mono func, boolean, integer, number, string;
extern const mono record;

template<class T>
struct Forall {
  var arg;
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
  list<var> bound() const;

  std::string show(repr_type repr={}) const;
};

using forall = Forall<poly>;

// system F types
template<class T>
struct Sigma: variant<Type<T>, Forall<T>> {
  using Sigma::variant::variant;

  template<class Func>
  friend auto map(const Sigma& self, Func func) {
    using type = typename std::result_of<Func(T)>::type;
    using result_type = Sigma<type>;
    return match(self,
                 [&](Type<T> self) -> result_type { return map(self, func); },
                 [&](Forall<T> self) -> result_type {
                   return Forall<type>{self.arg, func(self.body)};
                 });
  }
  
};

struct sigma: fix<Sigma, sigma> {
  using sigma::fix::fix;
  
};

using rho = Type<sigma>;

////////////////////////////////////////////////////////////////////////////////
struct context;

std::shared_ptr<context> make_context();

poly infer(std::shared_ptr<context> ctx, const ast::expr&);

}



#endif
