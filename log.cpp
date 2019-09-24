// -*- compile-command: "make log" -*-

#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <functional>

namespace log {
  enum level {
              debug = 0,
              info,
              warning,
              error,
              fatal,
              size
  };


  using handler = std::function<void(const std::string& line, const char* emitter)>;

  std::vector<handler>& handlers(log::level level) {
    static std::vector<handler> table[log::level::size];
    return table[level];
  }

  class emitter {
    std::string tag;
  public:
    friend std::ostream& operator<<(std::ostream& out, const emitter& self) {
      // return out << "__" << tag << "@";
    }

    friend std::istream& operator>>(std::istream& in, emitter& self) {
      char c;
      if(in >> c && c == '[') {
        while(in && in.get() != ']')
      }

      in.fail();
      return in;
    }

  };


  struct dispatch: std::stringbuf {
    std::stringstream ss;

    dispatch(log::level level, const char* emitter=nullptr):
      level(level),
      emitter(emitter) { }
    
    const log::level level;
    const char* emitter = nullptr;
    
    virtual int sync() {
      // start from leftovers
      ss << str();

      // split lines
      std::string line;
      while(std::getline(ss, line, '\n') && !ss.eof()) {
        for(const auto& h: handlers(level)) {
          h(line, emitter);
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

  

}

void default_handler(const std::string& line, const char* emitter=nullptr) {
  std::cout << "process: " << line << '\n';
}


int main(int, char** ) {
  log::dispatch buf(log::info);
  log::handlers(log::info).emplace_back(default_handler);
  
  std::ostream s(&buf);

  s << "yo" << std::endl << "what" << std::flush << "dobidou" << "\n" << std::flush;


  return 0;

}




