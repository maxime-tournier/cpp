#include "repl.hpp"

#include <cstdlib>
#include <readline/readline.h>
#include <readline/history.h>

#include "finally.hpp"

#include <string>
#include <cstring>


void repl(std::function<void(const char*)> handler,
          const char* prompt,
          const char* history) {
  char* buf;

  if(history) {
    read_history(history);
  }

  const auto lock = finally([history] {
    if(history) {
      write_history(history);
    }
  });
  
  
  while((buf = readline(prompt)) != nullptr) {
    if(std::strlen(buf) > 0) {
      add_history(buf);
    }

    std::string contents = buf;

    // readline malloc's a new buffer every time.
    std::free(buf);
    
    handler(contents.c_str());
  }
}

