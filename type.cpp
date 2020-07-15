#include "type.hpp"
#include "ast.hpp"

////////////////////////////////////////////////////////////////////////////////
namespace type {

const kind term = kind_constant::make("*");
const kind row = kind_constant::make("@");

const mono func = type_constant::make("->", term >>= term >>= term);

const mono unit = type_constant::make("()", term);
const mono boolean = type_constant::make("bool", term);
const mono integer = type_constant::make("int", term);
const mono number = type_constant::make("num", term);
const mono string = type_constant::make("str", term);

ref<kind_constant> kind_constant::make(symbol name) {
  return std::make_shared<const kind_constant>(name);
}

ref<type_constant> type_constant::make(symbol name, struct kind kind) {
  return std::make_shared<const type_constant>(name, kind);
}

kind kind::operator>>=(kind other) const {
  return ctor{*this, other};
}

bool kind::operator==(kind other) const {
    if(type() != other.type()) {
      return false;
    }
    return match(
        *this,
        [&](ctor lhs) {
          return std::tie(lhs.from, lhs.to) ==
                 std::tie(other.get<ctor>().from, other.get<ctor>().to);
        },
        [](auto) { return true; });
}

std::ostream& operator<<(std::ostream& out, struct kind self) {
  match(self,
        [&](ref<kind_constant> self) { out << self->name; },
        [&](ctor self) { out << self.from << " -> " << self.to; });
  return out;
}

std::ostream& operator<<(std::ostream& out, mono self) {
  return out << cata(self, [](Mono<std::string> self) {
    return match(self,
                 [](ref<type_constant> self) -> std::string { return self->name.repr; },
                 [](ref<var> self) -> std::string { return "<var>"; },
                 [](App<std::string> self) {
                   return self.ctor + " " + self.arg;
                 });
  });
}


struct kind mono::kind() const {
  return cata(*this, [](Mono<struct kind> self) {
    return match(self,
                 [](ref<type_constant> self) { return self->kind; },
                 [](ref<var> self) { return self->kind; },
                 [](App<struct kind> self) {
                   return self.ctor.get<ctor>().to;
                 });
  });
}


}

////////////////////////////////////////////////////////////////////////////////

#include "hamt.hpp"
#include <sstream>

namespace type {

template<class T>
static std::string quote(const T& self) {
  std::stringstream ss;
  ss << '"' << self << '"';
  return ss.str();
}

struct type_error: std::runtime_error {
  type_error(std::string what): std::runtime_error("type error: " + what) { }
};



// TODO inference monad

struct context {
  std::size_t depth = 0;
  hamt::map<symbol, poly> locals;

  mono instantiate(poly p) const {
    // TODO quantify + susbtitute
  };
  
};


ref<context> make_context() {
  return std::make_shared<context>();
}


static mono infer(ref<context> ctx, const ast::lit& self) {
  return match(self,
               [](long) { return integer; },
               [](double) { return number; },
               [](std::string) { return string; },
               [](bool) { return boolean; });
};


static mono infer(ref<context> ctx, const ast::var& self) {
  if(auto poly = ctx->locals.find(self.name)) {
    return ctx->instantiate(poly);
  }
  
  throw type_error("unbound variable: " + quote(self.name));
};


template<class T>
static mono infer(ref<context> ctx, const T&) {
  throw type_error("unimplemented: ");
}

mono infer(ref<context> ctx, const ast::expr& e) {
  return match(e, [=](const auto& self) { return infer(ctx, self); });
}

}

////////////////////////////////////////////////////////////////////////////////
