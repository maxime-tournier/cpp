#ifndef CPP_READLINE_HPP
#define CPP_READLINE_HPP

#include <readline/readline.h>
#include <readline/history.h>

#include <cstring>

template<class Cont>
static void repl(Cont cont, const char* prompt = "> ") {
  char* buf;
  while((buf = readline(prompt)) != nullptr) {
    if(std::strlen(buf) > 0) {
      add_history(buf);
    }
    
    cont(buf);
    
    // readline malloc's a new buffer every time.
    std::free(buf);
  }
}


#endif
