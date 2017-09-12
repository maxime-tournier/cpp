#include "parse.hpp"

#include <iostream>
#include <fstream>

namespace obj {

  using real = double;

  struct vec3 { real x, y, z; };

  struct vertex : vec3 {
    friend std::ostream& operator<<(std::ostream& out, const vertex& self) {
      return out << "v" << " " << self.x << " " << self.y << " " << self.z;
    }
  };
  
  struct normal : vec3 {

    friend std::ostream& operator<<(std::ostream& out, const normal& self) {
      return out << "vn" << " " << self.x << " " << self.y << " " << self.z;
    }

  };

  struct face {
    struct element {
      int vertex;
      int texcoord;
      int normal;

      friend std::ostream& operator<<(std::ostream& out, const element& self) {
        out << self.vertex;

        if(self.texcoord >= 0 || self.normal >= 0) out << "/";
        if(self.texcoord >= 0) out << self.texcoord;
        if(self.normal >= 0) out << "/" << self.normal;

        return out;
      }
    };

    element e1, e2, e3;
    std::vector<element> rest;

    template<class F>
    void each(F f) const {
      f(e1); f(e2); f(e3);
      for(const element& e : rest) { f(e); }
    }
    
    
    friend std::ostream& operator<<(std::ostream& out, const face& self) {
      out << 'f';
      
      self.each([&](const element& e) {
          out << ' ' << e;
        });
      
      return out;
    }
  };


  struct geometry {
    std::vector<vertex> vertices;
    std::vector<normal> normals;
    std::vector<face> faces;

    friend std::ostream& operator<<(std::ostream& out, const geometry& self) {
      for(const vertex& v : self.vertices) {
        out << v << '\n';
      }
      
      for(const normal& n : self.normals) {
        out << n << '\n';
      }

      for(const face& f : self.faces) {
        out << f << '\n';
      }

      return out;
    }
    
  };

  struct group {
    std::string name;
    geometry geo;
    
    friend std::ostream& operator<<(std::ostream& out, const group& self) {
      out << "g" << " " << self.name << '\n';
      return out << self.geo;
    }
    
  };
  

  struct object {
    std::string name;
    geometry geo;
    std::vector<group> groups;
    
    friend std::ostream& operator<<(std::ostream& out, const object& self) {
      out << "o" << " " << self.name << '\n';
      out << self.geo;
      
      for(const group& g : self.groups) {
        out << g;
      }

      return out;
    }

  };
  
  struct file {
    geometry geo;
    std::vector<group> groups;
    std::vector<object> objects;

    friend std::ostream& operator<<(std::ostream& out, const file& self) {

      out << self.geo;

      for(const group& grp : self.groups) {
        out << grp;
      }

      for(const object& obj : self.objects) {
        out << obj;
      }

      return out;
    }


    
    friend std::istream& operator>>(std::istream& in, file& self) {
      in >> std::noskipws;

      using namespace parse;

      static const auto space = chr(std::isblank);
      static const auto endl = chr<'\n'>();      

      static const auto comment = chr<'#'>() >> then(token(endl, !endl));
      
      static const auto skip = space | comment;

      static const auto g = tokenize(skip) << str("g");
      static const auto o = tokenize(skip) << str("o");      
      static const auto v = tokenize(skip) << str("v");
      static const auto vn = tokenize(skip) << str("vn");
      static const auto f = tokenize(skip) << str("f");            
      
      static const auto name = tokenize(skip) << +chr(std::isgraph);

      static const auto newline = tokenize(skip) << endl;


      static const auto group_def = g >> then(name) >> drop(newline);
      static const auto object_def = o >> then(name) >> drop(newline);      
      

      static const auto num = tokenize(skip) << lit<real>();

      static const auto vec3 = num >> [](real&& x) {
        return num >> [&](real&& y) {
          return num >> [&](real&& z) {
            return pure(obj::vec3{x, y, z});
          };
        };
      };
    
      static const auto vertex_def = debug("vertex") <<= v >> then(vec3) >> [](obj::vec3&& v) {
        return pure( static_cast<obj::vertex&&>(v) );
      } >> drop(newline);

      static const auto normal_def = debug("normal") <<= vn >> then(vec3) >> [](obj::vec3&& v) {
        return pure( static_cast<obj::normal&&>(v) );
      } >> drop(newline);

      
      static const auto integer = tokenize(skip) << lit<int>();
      static const auto slash = str("/", [](char c) { 
          return c == '/' || std::isdigit(c);
        });
      
      static const auto face_element = integer >> [&](int&& vertex) {
        return slash >> [&](const char*) {
          return integer >> [&](int&& texcoord) {
            // vertex/texcoord...
            return slash >> then(integer) >> [&](int&& normal) {
              // vertex/texcoord/normal
              return pure( face::element{vertex, texcoord, normal} );
            } | pure( face::element{vertex, texcoord, -1} );
            
          } | slash >> [&](const char* ) {
            // vertex/normal
            return integer >> [&](int&& normal) {
              return pure( face::element{vertex, -1, normal} );
            };
          };
        } | pure( face::element{vertex, -1, -1} );
        
      };


      static const auto face_def = debug("face") <<=
        f >> then(face_element) >> [](face::element&& e1) {
        return face_element >> [&](face::element&& e2) {
          return face_element >> [&](face::element&& e3) {
            
            return *face_element >> [&](std::vector<face::element>&& rest) {
              return pure( obj::face{e1, e2, e3, std::move(rest)} );
            };            
          };
        };
      } >> drop(newline);

      const auto geometry = debug("geometry") <<=
        pure( obj::geometry() ) >> [](obj::geometry&& self) {
        
        const auto line = debug("line") <<=
        ( vertex_def >> [&](obj::vertex&& v) {
          self.vertices.emplace_back(std::move(v));
          return pure(true);
        } | normal_def >> [&](obj::normal&& n) {
          self.normals.emplace_back(std::move(n));
          return pure(true);
        } | face_def >> [&](obj::face&& f) {
          self.faces.emplace_back(std::move(f));
          return pure(true);
        } | newline >> then(pure(true)) );
        
        // TODO don't build std::vector of bools here
        return *line >> [&](std::vector<bool>&& ) {
          return pure(std::move(self));
        };
        
      };
      

      static const auto group = debug("group") <<=
        group_def >> [&](std::vector<char>&& name) {
        return geometry >> [&](obj::geometry&& geo) {
          obj::group self;
          self.name = {name.begin(), name.end()};
          self.geo = std::move(geo);
          return pure(self);
        };
      };


      static const auto object = debug("object") <<=
        object_def >> [&](std::vector<char>&& name) {
        return geometry >> [&](obj::geometry&& geo) {
          return *group >> [&](std::vector<obj::group>&& groups) {
            
            obj::object self;
            self.name = {name.begin(), name.end()};
            self.geo = std::move(geo);
            self.groups = std::move(groups);
            
            return pure(self);
          };

        };
      };


      static const auto file = debug("file") <<=
        geometry >> [&](obj::geometry&& geo) {
        return *group >> [&](std::vector<obj::group>&& groups) {
          return *object >> [&](std::vector<obj::object>&& objects) {
            obj::file self;
            self.geo = std::move(geo);
            self.groups = std::move(groups);
            self.objects = std::move(objects);

            return pure(self);
          };
        };
        
      };

      static const auto parser = file;

      if( auto result = parser(in) ) {
        std::clog << "success" << std::endl;
        // std::cout << self << std::endl;
        std::cout << result.get() << std::endl;
      } else {
        throw std::logic_error("parse error");
      }
      
      return in;
    }

  };

}




int main(int argc, char** argv) {

  obj::file f;

  if(argc < 2) {
    std::cerr << "usage: " << argv[0] << " <objfile>" << std::endl;
    return 1;
  }

  std::ifstream in(argv[1]);
  if(!in.good()) {
    std::cerr << "file error" << std::endl;
    return 1;
  }

  in >> f;
  
  return 0;
}
