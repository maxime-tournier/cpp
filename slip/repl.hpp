#ifndef CPP_READLINE_HPP
#define CPP_READLINE_HPP

#include <readline/readline.h>
#include <readline/history.h>

#include "finally.hpp"

#include <functional>
#include <string>

void repl(std::function<void(const char*)> handler,
          const char* prompt = "> ",
          const char* history=nullptr);

#endif
