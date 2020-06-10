// #define PARSE_ENABLE_DEBUG
#include "parser.hpp"

#include <fstream>
#include <iostream>

namespace obj {
  
using real = double;

struct vec3 {
  real x, y, z;
};

struct vertex: vec3 {
  using vec3::operator=;
  
  friend std::ostream& operator<<(std::ostream& out, const vertex& self) {
    return out << "v " << self.x << " " << self.y << " " << self.z;
  }
};

struct normal: vec3 {
  using vec3::operator=;
  
  friend std::ostream& operator<<(std::ostream& out, const normal& self) {
    return out << "vn " << self.x << " " << self.y << " " << self.z;
  }
};

struct face {
  
  struct element {
    std::size_t vertex;
    std::size_t texcoord;
    std::size_t normal;

    friend std::ostream& operator<<(std::ostream& out, const element& self) {
      out << self.vertex;

      if(self.texcoord != -1 || self.normal != -1) {
        out << "/";
      }
      
      if(self.texcoord != -1) {
        out << self.texcoord;
      }
      
      if(self.normal != -1) {
        out << "/" << self.normal;
      }

      return out;
    }
  };

  std::deque<element> elements;

  friend std::ostream& operator<<(std::ostream& out, const face& self) {
    out << 'f';

    for(const auto& element: self.elements) {
      out << ' ' << element;
    };

    return out;
  }
};


struct geometry {
  std::vector<vertex> vertices;
  std::vector<normal> normals;
  std::vector<face> faces;

  // TODO materials

  friend std::ostream& operator<<(std::ostream& out, const geometry& self) {
    for(const auto& v: self.vertices) {
      out << v << '\n';
    }

    for(const auto& n: self.normals) {
      out << n << '\n';
    }

    for(const auto& f: self.faces) {
      out << f << '\n';
    }

    return out;
  }
};

struct group {
  std::string name;
  geometry geo;

  friend std::ostream& operator<<(std::ostream& out, const group& self) {
    out << "g"
        << " " << self.name << '\n';
    return out << self.geo;
  }
};


struct object {
  std::string name;
  geometry geo;
  std::vector<group> groups;

  friend std::ostream& operator<<(std::ostream& out, const object& self) {
    out << "o "<< self.name << '\n';
    out << self.geo;

    for(const auto& g: self.groups) {
      out << g;
    }

    return out;
  }
};

struct file {
  geometry geo;
  std::deque<group> groups;
  std::deque<object> objects;
  
  friend std::ostream& operator<<(std::ostream& out, const file& self) {
    out << self.geo;

    for(const auto& grp: self.groups) {
      out << grp;
    }

    for(const auto& obj: self.objects) {
      out << obj;
    }

    return out;
  }


  friend std::istream& operator>>(std::istream& in, file& self) {
    in >> std::noskipws;

    using namespace parser;

    static const auto space = pred(std::isblank);

    static const auto endl = single('\n');
    static const auto not_endl = pred([](int x) -> int { return x != '\n'; });

    static const auto comment = single('#') >> skip(not_endl) >> endl;
    
    static const auto separator = space | comment;
    
    static const auto g = token(keyword("g"), separator);
    static const auto o = token(keyword("o"), separator);
    static const auto v = token(keyword("v"), separator);
    static const auto vn = token(keyword("vn"), separator);
    static const auto f = token(keyword("f"), separator);
    static const auto mtllib = token(keyword("mtllib"), separator);
    
    static const auto name = token(plus(pred(std::isgraph)));
    static const auto newline = token(endl);

    static const auto section = [](auto what) {
      return what >> name >>= drop(newline);
    };
    
    static const auto number = token(_double);
    static const auto index = token(_unsigned_long);
    
    static const auto vec3 = number >>= [=](real x) {
      return number >>= [=](real y) {
        return number >>= [=](real z) {
          return unit(obj::vec3{x, y, z}); };
      };
    };
    
    static const auto vertex_def = v >> vec3 >>= [](obj::vec3 v) {
      return unit(obj::vertex() = v) >>= drop(newline);
    };

    static const auto normal_def = vn >> vec3 >>= [](obj::vec3 v) {
      return unit(obj::normal() = v) >>= drop(newline);
    };

    static const auto slash = token(single('/'));
    
    static const auto element = index >>= [=](std::size_t vertex) {
      return slash >> index >>= [=](std::size_t texcoord) {
        return slash >> index >>= [=](std::size_t normal) {
          return unit(face::element{vertex, texcoord, normal});
        };
      };
    };

    static const auto face_def = f >> kleene(element) >>= drop(newline);
    
    // const auto geometry =  pure(obj::geometry()) >> [](obj::geometry&& self) {
    //       const auto line =
    //           debug("line") <<= vertex_def >> [&](obj::vertex&& v) {
    //             self.vertices.emplace_back(std::move(v));
    //             return pure(true);
    //           } | normal_def >> [&](obj::normal&& n) {
    //             self.normals.emplace_back(std::move(n));
    //             return pure(true);
    //           } | face_def >> [&](obj::face&& f) {
    //             self.faces.emplace_back(std::move(f));
    //             return pure(true);
    //           } | mtllib_def >> [&](std::vector<char>&& name) {
    //             return pure(true);
    //           } | (newline, pure(true));

    //       // TODO don't build std::vector of bools here
    //       return *line >>
    //              [&](std::vector<bool>&&) { return pure(std::move(self)); };
    //     };


    // static const auto group = debug("group") <<=
    //     group_def >> [&](std::vector<char>&& name) {
    //       return geometry >> [&](obj::geometry&& geo) {
    //         obj::group self;
    //         self.name = {name.begin(), name.end()};
    //         self.geo = std::move(geo);
    //         return pure(self);
    //       };
    //     };


    // static const auto object = debug("object") <<=
    //     object_def >> [&](std::vector<char>&& name) {
    //       return geometry >> [&](obj::geometry&& geo) {
    //         return *group >> [&](std::vector<obj::group>&& groups) {
    //           obj::object self;
    //           self.name = {name.begin(), name.end()};
    //           self.geo = std::move(geo);
    //           self.groups = std::move(groups);

    //           return pure(self);
    //         };
    //       };
    //     };


    // static const auto file = debug("file") <<=
    //     geometry >> [&](obj::geometry&& geo) {
    //       return *group >> [&](std::vector<obj::group>&& groups) {
    //         return *object >> [&](std::vector<obj::object>&& objects) {
    //           obj::file self;
    //           self.geo = std::move(geo);
    //           self.groups = std::move(groups);
    //           self.objects = std::move(objects);

    //           return pure(self);
    //         };
    //       };
    //     };

    // static const auto parser = file;

    // if(auto result = parser(in)) {
    //   self = std::move(result.get());
    // } else {
    //   throw std::runtime_error("parse error");
    // }

    return in;
  }
};

} // namespace obj


int main(int argc, char** argv) {
  obj::file f;

  if(argc < 2) {
    std::cerr << "usage: " << argv[0] << " <objfile>" << std::endl;
    return 1;
  }

  std::ifstream in(argv[1]);
  if(!in.good()) {
    std::cerr << "file open error" << std::endl;
    return 1;
  }

  std::stringstream ss;
  ss << in.rdbuf();

  try {
    ss >> f;
  } catch(std::runtime_error& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
