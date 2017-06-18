#include "parse.hpp"

#include "sexpr.hpp"
#include "syntax.hpp"

namespace slip {

  using namespace parse;
  
  static const auto space = debug("space") >>= chr<std::isspace>();
  static const auto endl = chr('\n');
  static const auto comment = no_skip(( chr(';'), kleene(!endl), endl));

  static const auto skip = *(space | comment);
  

  parse::any<sexpr> skipper() {
    return skip >> then(pure( sexpr(unit()) ));
  }
  
  parse::any<sexpr> parser() {
    
    const auto alpha = debug("alpha") >>= chr<std::isalpha>();
    const auto alnum = debug("alnum") >>= chr<std::isalnum>();

    const auto dquote = chr('"');

    const auto special = debug("special") >>= chr("!$%&*/:<=>?~_^@#-+");

    const auto initial = alpha | special;
    const auto subsequent = alnum | special;    
    
    const auto token = parse::tokenize(skip);
    
    const auto number = debug("number") >>=
      token >>=
      lit<slip::real>() >> [](slip::real&& x) {
      integer n = x;
      if(n == x) return pure<sexpr>(n);
      return pure<sexpr>(x);
    };
  
  
    const auto symbol = debug("symbol") >>=
      token >>=
      no_skip(initial >> [subsequent](char first) {
          return *subsequent >> [first](std::deque<char>&& chars) {
            chars.emplace_front(first);
            const slip::string str(chars.begin(), chars.end());
            return pure<sexpr>( slip::symbol(str) );
          };
        });

    // TODO escaped strings
    const auto string = debug("string") >>=
      token >>=
      no_skip( (dquote, *!dquote) >> [dquote](std::deque<char>&& chars) {
          ref<slip::string> str = make_ref<slip::string>(chars.begin(), chars.end());
          return dquote, pure<sexpr>(str);
        });
  
    const auto atom = debug("atom") >>=
      (string | symbol | number);
    
    struct expr_tag;
    rec<sexpr, expr_tag> expr;


    const auto lparen = token >>= chr('(');
    const auto rparen = token >>= chr(')');    
    
    const auto list = debug("list") >>=
      no_skip( (lparen, *expr) >> [rparen](std::deque<sexpr>&& terms) {
          return rparen, pure<sexpr>(slip::make_list<sexpr>(terms.begin(), terms.end()));
        });

    
    const auto quote = chr('\'');
    const auto backquote = chr('`');
    const auto comma = chr(',');        
    
    const auto quoted_expr = no_skip(quote >> then(expr) >> [](sexpr&& e) {
        return pure<sexpr>( slip::symbol(kw::quote) >>= e >>= sexpr::list() );
      });

    const auto quasiquoted_expr = no_skip(backquote >> then(expr) >> [](sexpr&& e) {
        return pure<sexpr>( slip::symbol(kw::quasiquote) >>= e >>= sexpr::list() );
      });

    const auto unquote_expr = no_skip(comma >> then(expr) >> [](sexpr&& e) {
        return pure<sexpr>( slip::symbol(kw::unquote) >>= e >>= sexpr::list() );
      });

    
    
    expr = debug("expr") >>=
      atom | list | quoted_expr | quasiquoted_expr | unquote_expr;
    
    return expr;
  }

}
