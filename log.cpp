// -*- compile-command: "make log" -*-

#include <iostream>
#include <sstream>
#include <vector>
#include <memory>

enum level {
            debug,
            info,
            warning,
            error,
            fatal,
};

struct handler {
  virtual ~handler() { }

  virtual void process(const std::string& line, const char* emitter=nullptr) = 0;
};


struct dispatch: std::stringbuf {
  std::vector<std::unique_ptr<handler>> handlers;
  
  std::stringstream ss;
  
  virtual int sync() {
    // start from leftovers
    ss << str();

    // split lines
    std::string line;
    while(std::getline(ss, line, '\n') && !ss.eof()) {
      for(const auto& h: handlers) {
        h->process(line);
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


struct default_handler: handler {
  void process(const std::string& line, const char* emitter=nullptr) override {
    std::cout << "process: " << line << '\n';
  }
};


int main(int, char** ) {
  dispatch buf;
  buf.handlers.emplace_back(new default_handler);

  std::ostream s(&buf);

  s << "yo" << std::endl << "what" << std::flush << "dobidou" << "\n" << std::flush;


  return 0;

}




