// #define PARSE_ENABLE_DEBUG
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
    
    const auto initial = token(alpha | special, skip);
    const auto subsequent = alnum | special;    

    const auto number = debug("number") <<= 
      token(lit<slip::real>(), skip) >> [](slip::real&& x) {

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
      (token(dquote, skip), *!dquote) >> [](std::vector<char>&& chars) {
      ref<slip::string> str = make_ref<slip::string>(chars.begin(), chars.end());
      return dquote, pure<sexpr>(str);
    };
    
    const auto atom = debug("atom") <<= (string | symbol | number);
    
    struct expr_tag;
    rec<sexpr, expr_tag> expr;
    
    const auto list = (lparen, *expr) >> [](std::vector<sexpr>&& terms) {
      return rparen, pure<sexpr>(slip::make_list<sexpr>(terms.begin(), terms.end()));
    };
    
    const auto quote = chr<'\''>();
    const auto backquote = chr<'`'>();
    const auto comma = chr<','>();        
    
    expr = debug("expr") <<= list | atom; 
    
    return expr | parse::error<sexpr, parse_error>();
  }

}
