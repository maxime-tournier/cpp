#include "parse.hpp"

#include "value.hpp"
#include "eval.hpp"

namespace lisp {

  parse::any<value> parser() {
    
    using namespace parse;

    const auto alpha = debug("alpha") >>= chr<std::isalpha>();
    const auto alnum = debug("alnum") >>= chr<std::isalnum>();
    const auto space = debug("space") >>= chr<std::isspace>();
  
    const auto dquote = chr('"');
  
    // const auto endl = chr('\n');
    // const auto comment = (chr(';'), kleene(!endl), endl);

    const auto as_value = cast<value>();
  
    const auto real = debug("real") >>= 
      lit<lisp::real>() >> as_value;
  
    const auto integer = debug("integer") >>=
      lit<lisp::integer>() >> as_value;
  
    const auto symbol = debug("symbol") >>=
      no_skip(alpha >> [alnum](char first) {
          return *alnum >> [first](std::deque<char>&& chars) {
            chars.emplace_front(first);
            const lisp::string str(chars.begin(), chars.end());
            return pure<value>( lisp::symbol(str) );
          };
        });
    
    const auto string = debug("string") >>=
      no_skip( (dquote, *!dquote) >> [dquote](std::deque<char>&& chars) {
          ref<lisp::string> str = make_ref<lisp::string>(chars.begin(), chars.end());
          return dquote, pure<value>(str);
        });
  
    const auto atom = debug("atom") >>=
      string | symbol | integer | real;
    
    struct expr_tag;
    rec<value, expr_tag> expr;
  
    const auto list = debug("list") >>=
      no_skip( (chr('('), *space, expr % +space) >> [space](std::deque<value>&& terms) {
          return *space, chr(')'), pure<value>(lisp::make_list(terms.begin(), terms.end()));
        });

    const auto quote = chr('\'');
    const auto quoted_expr = no_skip(quote >> then(expr) >> [](value&& e) {
        return pure<value>( lisp::symbol("quote") >>= e >>= nil );
      });
    
    expr = debug("expr") >>=
      atom | list | quoted_expr;
    
    return expr;
  }

}
