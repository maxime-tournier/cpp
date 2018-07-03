#ifndef CPP_VIEWER_HPP
#define CPP_VIEWER_HPP

#include <functional>

struct viewer {

  using callback_type = std::function< void() >;
  callback_type animate, draw, init;

  int run(int argc, char** argv);
};




#endif