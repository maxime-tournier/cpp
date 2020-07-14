#include "infer.hpp"
#include "ast.hpp"

////////////////////////////////////////////////////////////////////////////////
namespace infer {



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

////////////////////////////////////////////////////////////////////////////////
struct type_error: std::runtime_error {
  type_error(std::string what): std::runtime_error("type error: " + what) { }
};

mono infer(context ctx, ast::expr e) {
  return match(e,
               [](ast::lit self) {
                 return match(self,
                              [](long) { return integer; },
                              [](double) { return number; },
                              [](std::string) { return string; },
                              [](bool) { return boolean; });
                              
               },
               [](auto) -> mono {
                 throw type_error("unimplemented");
               });
}

}

////////////////////////////////////////////////////////////////////////////////
int main(int, char**) {

  return 0;
};
