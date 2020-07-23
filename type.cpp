#include "type.hpp"
#include "ast.hpp"
#include "either.hpp"

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

list<ref<var>> mono::vars() const {
  return cata(*this, [](Mono<list<ref<var>>> self) {
    return match(self,
                 [](ref<var> self) { return self %= list<ref<var>>{}; },
                 [](App<list<ref<var>>> self) { return concat(self.ctor, self.arg); },
                 [](ref<type_constant> self) { return list<ref<var>>{}; });
  });
}


mono poly::body() const {
  return cata(*this, [](Poly<mono> self) {
    return match(self,
                 [](Forall<mono> self) { return self.body; },
                 [](mono self) { return self; });
  });
}



list<ref<var>> poly::bound() const {
  return cata(*this, [](Poly<list<ref<var>>> self) {
    return match(self,
                 [](Forall<list<ref<var>>> self) { return self.arg %= self.body; }, 
                 [](mono self) { return list<ref<var>>{}; });
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

////////////////////////////////////////////////////////////////////////////////
// substitutions
////////////////////////////////////////////////////////////////////////////////

class substitution {
  hamt::map<const var*, mono> table;
public:
  
  substitution link(const var* a, mono ty) const {
    return {table.set(a, std::move(ty))};
  }
  
  mono operator()(mono ty) const {
    return cata(ty, [&](Mono<mono> self) {
      return match(self,
                   [&](ref<var> self) -> mono {
                     if(auto res = table.find(self.get())) {
                       return *res;
                     }
                     return self;
                   },
                   [](auto self) -> mono { return self; });                   
    });
  }

  // composition
  friend substitution operator<<(substitution lhs, substitution rhs) {
    rhs.table.iter([&](auto var, auto mono) {
      lhs.table = std::move(lhs.table).set(var, std::move(mono));
    });
    
    return {std::move(lhs.table)};
  }
  
};

////////////////////////////////////////////////////////////////////////////////
// inference monad
////////////////////////////////////////////////////////////////////////////////

template<class T>
struct success {
  using value_type = T;
  value_type value;
  substitution sub;
};

template<class T>
static success<T> make_success(T value, substitution sub) {
  return {value, sub};
};

template<class T>
struct result: either<type_error, success<T>> {
  using result::either::either;
  
  template<class Func>
  static auto bind(const result& self, const Func& func) {
    return result::either::bind(self, [&](auto outer) {
      using result_type = typename std::result_of<Func(typename result::either::value_type)>::type;
      using type = typename result_type::value_type::value_type;
      
      return match(func(outer.value),
                   [](type_error error) -> result_type { return error; },
                   [&](success<type> inner) -> result_type {
                     return make_success(inner.value, outer.sub << inner.sub);
                   });
    });
  }

  template<class Func>
  static auto map(const result& self, const Func& func) {
    return result::either::map(self, [&](auto outer) {
      using type = typename std::result_of<Func(typename result::either::value_type)>::type;

      return make_success(func(outer.value), outer.sub);
    });
  }
  
};



struct context {
  std::size_t depth = 0;
  hamt::map<symbol, poly> locals;

  ref<var> fresh(struct kind kind=term) const {
    return std::make_shared<var>(depth, kind);
  }
  
  mono instantiate(poly p, struct kind kind=term) const {
    const substitution sub =
        foldr(p.bound(), substitution{}, [&](ref<var> a, substitution sub) {
          return sub.link(a.get(), fresh(kind));
        });

    return sub(p.body());
  };

  poly generalize(mono m) const {
    
  }
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
