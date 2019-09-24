// -*- compile-command: "CXXFLAGS=-std=c++14 make log" -*-

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


  using handler = std::function<void(const std::string& line)>;

  std::vector<handler>& handlers(log::level level) {
    static std::vector<handler> table[log::level::size];
    return table[level];
  }

  struct emitter {
    std::string tag;

    friend std::ostream& operator<<(std::ostream& out, const emitter& self) {
      return out << "[" << self.tag << "]";
    }

    friend std::istream& operator>>(std::istream& in, emitter& self) {
      const auto pos = in.tellg();
      char c;
      if((in >> c) && c == '[') {
        std::string tag;
        if(std::getline(in, tag, ']')) {
          self.tag = tag;
          return in;
        }
      }

      in.clear();
      in.seekg(pos);
      in.setstate(std::ios::failbit);
      
      return in;
    }
    
  };
  

  struct dispatch: std::stringbuf {
    std::stringstream ss;

    dispatch(log::level level):
      level(level) { }
    
    const log::level level;
    
    virtual int sync() {
      // start from leftovers
      ss << str();

      // split lines
      std::string line;
      while(std::getline(ss, line, '\n') && !ss.eof()) {
        for(const auto& h: handlers(level)) {
          h(line);
        }
      }
    
      // reset stringstream with leftovers, if any
      ss = std::stringstream();
      ss << line;

      // clear buffer
      str("");
    
      return 0;
    }
  };


}

void default_handler(const std::string& line) {
  std::stringstream ss(line);
  log::emitter em;
  if(ss >> em) {
    std::cout << "emitter: " << em.tag << " ";
  }
  std::cout << "message: " << ss.rdbuf() << "\n";
}


int main(int, char** ) {
  log::dispatch buf(log::info);
  log::handlers(log::info).emplace_back(default_handler);
  
  std::ostream s(&buf);

  s << log::emitter{"michel"} << "yo" << std::endl
    << "what" << std::flush << "dobidou" << "\n" << std::flush;


  return 0;

}




