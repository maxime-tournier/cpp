#ifndef CPP_READLINE_HPP
#define CPP_READLINE_HPP

#include <functional>

void repl(std::function<void(const char*)> handler,
          const char* prompt = "> ",
          const char* history=nullptr);

#endif
