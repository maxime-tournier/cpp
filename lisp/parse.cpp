#include "parse.hpp"

#include "sexpr.hpp"
#include "syntax.hpp"

namespace lisp {

  parse::any<sexpr> parser() {
    
    using namespace parse;

    const auto alpha = debug("alpha") >>= chr<std::isalpha>();
    const auto alnum = debug("alnum") >>= chr<std::isalnum>();
    const auto space = debug("space") >>= chr<std::isspace>();
  
    const auto dquote = chr('"');
  
    // const auto endl = chr('\n');
    // const auto comment = (chr(';'), kleene(!endl), endl);

    const auto as_sexpr = cast<sexpr>();
  
    const auto real = debug("real") >>= 
      lit<lisp::real>() >> as_sexpr;
  
    const auto integer = debug("integer") >>=
      lit<lisp::integer>() >> as_sexpr;
  
    const auto symbol = debug("symbol") >>=
      no_skip(alpha >> [alnum](char first) {
          return *alnum >> [first](std::deque<char>&& chars) {
            chars.emplace_front(first);
            const lisp::string str(chars.begin(), chars.end());
            return pure<sexpr>( lisp::symbol(str) );
          };
        });
    
    const auto string = debug("string") >>=
      no_skip( (dquote, *!dquote) >> [dquote](std::deque<char>&& chars) {
          ref<lisp::string> str = make_ref<lisp::string>(chars.begin(), chars.end());
          return dquote, pure<sexpr>(str);
        });
  
    const auto atom = debug("atom") >>=
      string | symbol | real | integer;
    
    struct expr_tag;
    rec<sexpr, expr_tag> expr;
  
    const auto list = debug("list") >>=
      no_skip( (chr('('), *space, expr % +space) >> [space](std::deque<sexpr>&& terms) {
          return *space, chr(')'), pure<sexpr>(lisp::make_list<sexpr>(terms.begin(), terms.end()));
        });

    const auto quote = chr('\'');
    const auto quoted_expr = no_skip(quote >> then(expr) >> [](sexpr&& e) {
        return pure<sexpr>( lisp::symbol(kw::quote) >>= e >>= sexpr::list() );
      });
    
    expr = debug("expr") >>=
      atom | list | quoted_expr;
    
    return expr;
  }

}
