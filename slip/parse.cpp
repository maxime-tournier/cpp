#include "parse.hpp"

#include "sexpr.hpp"
#include "syntax.hpp"

namespace slip {

  using namespace parse;
  
  static const auto space = debug("space") <<= chr(std::isspace);
  static const auto endl = chr<'\n'>();
  static const auto comment = (chr<';'>(), *!endl, endl);
  
  static const auto skip = (space | comment);
  
  static const auto lparen = debug("(") <<= token(chr<'('>(), skip);
  static const auto rparen = debug(")") <<= token(chr<')'>(), skip);    
  


  static const auto dquote = chr<'"'>();  
  
  static const auto alpha = chr(std::isalpha);
  static const auto alnum = chr(std::isalnum);

  parse::any<sexpr> parser() {
    
    const auto special = chr("!$%&*/:<=>?~_^@#-+");
    
    const auto initial = alpha | special;
    const auto subsequent = alnum | special;    
    
    const auto number = debug("number") <<= 
      lit<slip::real>() >> [](slip::real&& x) {
      integer n = x;
      if(n == x) return pure<sexpr>(n);
      return pure<sexpr>(x);
    };
  
  
    const auto symbol = debug("symbol") <<=
      initial >> [subsequent](char first) {
          return *subsequent >> [first](std::vector<char>&& chars) {
            slip::string res;
            res += first;
            std::copy(chars.begin(), chars.end(), std::back_inserter(res));
            return pure<sexpr>( slip::symbol(res) );
          };
    };

    // TODO escaped strings
    const auto string = debug("string") <<=
      ((dquote, *!dquote) >> [](std::vector<char>&& chars) {
      ref<slip::string> str = make_ref<slip::string>(chars.begin(), chars.end());
      return dquote, pure<sexpr>(str);
      });
    
    const auto atom = debug("atom") <<= (string | symbol | number);
    
    struct expr_tag;
    rec<sexpr, expr_tag> expr;
    
    const auto list = (lparen, *expr) >> [](std::vector<sexpr>&& terms) {
      return rparen, pure<sexpr>(slip::make_list<sexpr>(terms.begin(), terms.end()));
    };
    
    
    const auto quote = chr<'\''>();
    const auto backquote = chr<'`'>();
    const auto comma = chr<','>();        
    
    // const auto quoted_expr = quote >> then(expr) >> [](sexpr&& e) {
    //   return pure<sexpr>( slip::symbol(kw::quote) >>= e >>= sexpr::list() );
    // };
    
    // const auto quasiquoted_expr = backquote >> then(expr) >> [](sexpr&& e) {
    //     return pure<sexpr>( slip::symbol(kw::quasiquote) >>= e >>= sexpr::list() );
    //   };

    // const auto unquote_expr = comma >> then(expr) >> [](sexpr&& e) {
    //   return pure<sexpr>( slip::symbol(kw::unquote) >>= e >>= sexpr::list() );
    // };

    expr = debug("expr") <<= list | atom; // atom; //  | list; //| quoted_expr | quasiquoted_expr | unquote_expr;
    
    return expr;
  }

}
