#include "sexpr.hpp"

parser::monad<sexpr> sexpr::parse() {
  using namespace parser;

  const auto lparen = token(single('('));
  const auto rparen = token(single(')'));

  const auto quote = single('"');
  const auto not_quote = pred([](char x) { return x != '"'; });

  const auto backslash = single('\\');

  const auto escaped = backslash >> character;

  const auto chars = kleene(escaped | not_quote) >>= [](auto chars) {
    std::string value(chars.begin(), chars.end());
    return pure(sexpr(std::move(value)));
  };

  const auto string = token(quote) >> chars >>= drop(quote);

  const auto cast = [](auto value) { return sexpr(value); };

  const auto number = longest(map(_long, cast), map(_double, cast));

  const auto space = pred(std::isspace);

  const auto first = pred(std::isalpha);
  const auto next = pred(std::isalnum);

  const auto sym = first >>= [=](char first) {
    return kleene(next) >>= [=](auto nexts) {
      nexts.emplace_front(first);
      std::string repr(nexts.begin(), nexts.end());
      symbol s(repr.c_str());
      return pure(s);
    };
  };

  const auto atom = number | string | (sym |= cast);

  const auto list = [=](auto parser) {
    auto inner = ((parser % space) | pure(std::deque<sexpr>{})) |=
        [](auto items) {
          // TODO use move iterator
          return sexpr(make_list(items.begin(), items.end()));
        };

    return lparen >> inner >>= drop(rparen);
  };

  const auto dot = token(single('.'));

  const auto peek = [](auto parser) {
    return [parser](range in) -> parser::result<bool> {
      return make_success(bool(parser(in)), in);
    };
  };

  const auto with_source = [](auto expr) {
    return cursor >>= [=](const char* first) {
      return expr >>= [=](sexpr s) {
        return cursor >>= [=](const char* last) {
          sexpr r = s;
          r.source = {first, last};
          return pure(r);
        };
      };
    };
  };
  
  
  const auto expr = parser::fix<sexpr>([=](auto& self) {
    return with_source((atom | list(self)) >>= [=](sexpr e) {
      return ((dot >> (sym % dot)) >>=
              [=](auto attrs) {
                return pure(foldl(e,
                                  make_list(attrs.begin(), attrs.end()),
                                  [](sexpr e, symbol name) -> sexpr {
                                    return attrib{e, name};
                                  }));
              }) | pure(e);
    });
  });

  return expr;

}
