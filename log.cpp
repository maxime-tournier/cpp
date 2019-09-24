// -*- compile-command: "make log" -*-

#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <functional>

enum level {
            debug,
            info,
            warning,
            error,
            fatal,
};


using handler = std::function<void(const std::string& line, const char* emitter)>;


struct dispatch: std::stringbuf {
  std::vector<handler> handlers;
  
  std::stringstream ss;
  
  virtual int sync() {
    // start from leftovers
    ss << str();

    // split lines
    std::string line;
    while(std::getline(ss, line, '\n') && !ss.eof()) {
      for(const auto& h: handlers) {
        h(line, nullptr);
      }
    }
    
    // reset stringstream with leftovers, if any
    ss = {};
    ss << line;

    // clear buffer
    str("");
    
    return 0;
  }
  
};


void default_handler(const std::string& line, const char* emitter=nullptr) {
  std::cout << "process: " << line << '\n';
}


int main(int, char** ) {
  dispatch buf;
  buf.handlers.emplace_back(default_handler);

  std::ostream s(&buf);

  s << "yo" << std::endl << "what" << std::flush << "dobidou" << "\n" << std::flush;


  return 0;

}




