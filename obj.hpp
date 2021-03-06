#ifndef OBJ_HPP
#define OBJ_HPP

#include "parser.hpp"


namespace obj {
  
using real = double;

struct texcoord {
  real u, v;
};

struct vec3 {
  real x, y, z;
};

struct vertex: vec3 {
  vertex(vec3 self): vec3(self) { }
  
  friend std::ostream& operator<<(std::ostream& out, const vertex& self) {
    return out << "v " << self.x << " " << self.y << " " << self.z;
  }
};

struct normal: vec3 {
  normal(vec3 self): vec3(self) { }
  
  friend std::ostream& operator<<(std::ostream& out, const normal& self) {
    return out << "vn " << self.x << " " << self.y << " " << self.z;
  }
};

struct face {
  
  struct element {
    std::size_t data[0];
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
  std::deque<vertex> vertices;
  std::deque<normal> normals;
  std::deque<texcoord> texcoords;  
  std::deque<face> faces;

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
  
  obj::geometry geometry;
  std::deque<group> groups;
  std::deque<object> objects;
  
  friend std::ostream& operator<<(std::ostream& out, const file& self) {
    out << self.geometry;

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

    // const auto space = pred(std::isblank);
    const auto space = pred(std::isblank);    

    const auto endl = single('\n');
    const auto not_endl = pred([](int x) -> int { return x != '\n'; });

    const auto comment = single('#') >> skip(not_endl) >> endl;
    const auto separator = space | comment;
    
    const auto g = token(keyword("g"), space);
    const auto o = token(keyword("o"), space);

    const auto v = token(keyword("v"), space);
    const auto vn = token(keyword("vn"), space);
    const auto vt = token(keyword("vt"), space);
    
    const auto f = token(keyword("f"), space);
    const auto mtllib = token(keyword("mtllib"), space);
    
    const auto name = token(plus(pred(std::isgraph)), space);
    const auto newline = token(endl, space);
    
    const auto section = [=](auto what) {
      return what >> name >>= drop(newline);
    };
    
    const auto number = token(_double);
    
    const auto vec3 = number >>= [=](real x) {
      return number >>= [=](real y) {
        return number >>= [=](real z) {
          return pure(obj::vec3{x, y, z}); };
      };
    };
    
    const auto vertex_def = v >> vec3 >>= [=](obj::vec3 v) {
      return pure(obj::vertex(v)) >>= drop(newline);
    };

    const auto normal_def = vn >> vec3 >>= [=](obj::vec3 v) {
      return pure(obj::normal(v)) >>= drop(newline);
    };

    const auto texcoord_def = vt >> number >>= [=](auto u) {
      return number >>= [=](auto v) {
        return pure(obj::texcoord{u, v}) >>= drop(newline);
      };
    };

    
    const auto index = token(_unsigned_long);
    const auto slash = token(single('/'));
    
    const auto element = (index % slash) >>= [](auto indices) {
      face::element e;
      for(std::size_t i = 0; i < 3; ++i) {
        e.data[i] = i <= indices.size() ? indices[i] : 0;
      }

      return pure(e);
    };
    
    // TODO check there's at least three elements?
    const auto face_def = f >> plus(element) >>= [=](auto elements) {
      return pure(face{std::move(elements)}) >>= drop(newline);
    };

    const auto geometry = pure(obj::geometry{}) >>= [=](obj::geometry geo) {

      const auto append_to = [&](auto& where) {
        return [&](auto value) {
          where.emplace_back(value);
          return pure(true);
        };
      };
      
      const auto vertex = vertex_def >>= append_to(geo.vertices);
      const auto normal = normal_def >>= append_to(geo.normals);
      const auto texcoord = texcoord_def >>= append_to(geo.texcoords);
      const auto face = face_def >>= append_to(geo.faces);

      // TODO mtllib, parameters
      
      const auto line = token(vertex | normal | texcoord | face,
                              pred(std::isspace) | comment);
        
      return skip(line) >> pure(std::move(geo));
    };
    
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


    const auto file = geometry >>= [](auto geo) {
      obj::file file;
      file.geometry = std::move(geo);
      return pure(std::move(file));
    };
    
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
    const auto parser = file >>= drop(eos);

    const std::string contents = parser::read(in);
    self = run(parser, contents);
    return in;
  }
};

} // namespace obj


#endif
