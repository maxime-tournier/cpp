// -*- compile-command: "c++ -std=c++11 stl.cpp -o stl -lstdc++" -*-

#include <stdint.h>
#include <iostream>
#include <vector>
#include <fstream>


#include "parse.hpp"

namespace stl {

  union uint32 {
    char bytes[4];
    std::uint32_t value;
  };  

  std::istream& operator>>(std::istream& in, uint32& self) {
    return in.read(self.bytes, sizeof(self.bytes));
  }

  union real32 {
    char bytes[4];
    float value;
    static_assert(sizeof(float) == 4, "size error");
  };
  
  std::istream& operator>>(std::istream& in, real32& self) {
    // TODO deal with endianess here(yummy)
    return in.read(self.bytes, sizeof(self.bytes));
  }
  
  struct vec3 {
    float x, y, z;
  };


  struct normal : vec3 {
    using vec3::vec3;
    normal(const vec3& v) : vec3(v) { }
  };

  static std::ostream& operator<<(std::ostream& out, const normal& self) {
    return out << "normal " << self.x << ' ' << self.y << ' ' << self.z;
  }
  
  
  struct vertex : vec3 {
    using vec3::vec3;

    vertex(const vec3& v) : vec3(v) { }    
  };

  static std::ostream& operator<<(std::ostream& out, const vertex& self) {
    return out << "vertex " << self.x << ' ' << self.y << ' ' << self.z;
  }

  
  std::istream& operator>>(std::istream& in, vec3& self) {
    real32 tmp;
    for(float* ptr = &self.x; ptr < &self.x + 3; ++ptr) {
      in >> tmp;
      *ptr = tmp.value;
    }

    return in;
  }
  
  struct outer_loop {
    vertex v0, v1, v2;
  };

  std::istream& operator>>(std::istream& in, outer_loop& self) {
    return in >> self.v0 >> self.v1 >> self.v2;
  }
  
  static std::ostream& operator<<(std::ostream& out, const outer_loop& self) {
    return out << "outer loop" << '\n'
               << "  " << self.v0 << '\n'
               << "  " << self.v1 << '\n'
               << "  " << self.v2 << '\n'
               << "endloop";
  }
  

  union attribute_count {
    char bytes[2];
    std::uint16_t value;
  };

  std::istream& operator>>(std::istream& in, attribute_count& self) {
    return in.read(self.bytes, sizeof(self.bytes));
  }
  
  
  struct triangle {
    normal n;
    outer_loop loop;
    std::uint16_t attr;    
  };
  
  static std::ostream& operator<<(std::ostream& out, const triangle& self) {
    return out << "facet " << self.n << '\n'
               << self.loop << '\n'
               << "endfacet";
  }
  


  
  std::istream& operator>>(std::istream& in, triangle& self) {
    attribute_count attr;
    in >> self.n >> self.loop >> attr;
    self.attr = attr.value;
    return in;
  }

  
  

  struct file {
    std::string info;
    std::vector<triangle> triangles;
  };
  
  struct binary : file { };

  std::istream& operator>>(std::istream& in, binary& self) { 
    self = binary();
    
    char header[80];
    in.read(header, sizeof(header));

    self.info = header;
    
    uint32 size;
    in >> size;
    
    triangle t;
    attribute_count attr;
    
    for(std::uint32_t i = 0; i < size.value; ++i) {
      in >> t >> attr;
      self.triangles.emplace_back( std::move(t) );
    }
    
    return in;
  }


  struct stream_pos {
    std::istream& in;
    const std::istream::pos_type pos;
    
    stream_pos(std::istream& in)
      : in(in),
        pos(in.tellg()) {

    }

    ~stream_pos() {
      in.clear();
      in.seekg(pos);
    }
  };
  

  static bool is_binary(std::istream& in) {
    const stream_pos backup(in);
    
    in.seekg(0, std::ios::end);
    const std::size_t file_size = in.tellg();

    in.seekg(0, std::ios::beg);    
    char header[80];

    if(!in.read(header, sizeof(header)) ) {
      return false;
    }

    uint32 triangles;
    if(!in.read(triangles.bytes, sizeof(triangles.bytes))) {
      return false;
    }
    
    static const std::size_t float_size = 4;
    static const std::size_t vec3_size = 3 * float_size;
    static const std::size_t triangle_size = 4 * vec3_size + 2;
    static const std::size_t header_size = 80 + 4;

    const std::size_t expected_size = header_size + triangles.value * triangle_size;

    return file_size == expected_size;
  }


  struct ascii : file { };


  static std::ostream& operator<<(std::ostream& out, const ascii& self) {
    out << "solid " << self.info << '\n';

    for(const triangle& t : self.triangles) {
      out << t << '\n';
    }

    return out << "endsolid " << self.info << '\n';
  }
  
  std::istream& operator>>(std::istream& in, ascii& self) {
    using namespace parse;

    in >> std::noskipws;
    
    auto space = chr<' '>();
    auto newline = chr<'\n'>();
    auto eol = token(newline, space);
    
    // keywords
    auto solid = token(str("solid"), space);
    auto endsolid = token(str("endsolid"), space);    

    auto facet = token(str("facet"), space);
    auto endfacet = token(str("endfacet"), space);    

    auto normal = token(str("normal"), space);
    auto vertex = token(str("vertex"), space);    

    auto outer = token(str("outer"), space);
    auto loop = token(str("loop"), space);
    auto endloop = token(str("endloop"), space);        
    
    // floating point numbers
    using real_type = float;
    auto num = token(lit<real_type>(), space);

    // vec3
    auto vec3 = num >> [&](real_type&& x) {
      return num >> [&](real_type&& y) {
        return num >> [&](real_type&& z) {
          return pure( stl::vec3{x, y, z} );
        };
      };
    };

    // normal/vertex
    auto normal_def = normal >> then(space) >> then(vec3) >> [](stl::vec3&& v) {
      return pure( stl::normal(v) );
    } >> drop(eol);
    
    auto vertex_def = vertex >> then(space) >> then(vec3) >> [](stl::vec3&& v) {
      return pure( stl::vertex(v) );
    } >> drop(eol);    

    // loops
    auto outer_loop = outer >> then(space) >> then(loop);

    auto loop_def = outer_loop >> drop(eol) >>  
      then(vertex_def) >> [&](stl::vertex&& v0) {
      return vertex_def >> [&](stl::vertex&& v1) {
        return vertex_def >> [&](stl::vertex&& v2) {
          stl::outer_loop res = {v0, v1, v2};
          return pure(std::move(res));
        };
      };
    } >> drop(endloop) >> drop(eol);

    // facets
    auto facet_def = facet >> then(space) >> then(normal_def) >> [&](stl::vec3&& n) {
      return loop_def >> [&](stl::outer_loop&& loop) {

        stl::triangle res;
        res.loop = loop;
        res.n = n;

        return pure(std::move(res));
      };
    }
    >> drop(endfacet) >> drop(eol);

    // solids
    auto name = +!newline;
    
    auto solid_name = solid >> then(space) >> then(name) >> drop(eol);
    auto endsolid_name = endsolid >> then(space) >> then(name) >> drop(eol);    

    auto solid_def = solid_name >> [&](std::vector<char>&& sname) {
      
      return +facet_def >> [&](std::vector<triangle>&& triangles) {
        
        return endsolid_name >> [&](std::vector<char>&& ename) {

          if(ename != sname) {
            throw std::runtime_error("bad endsolid name");
          }
          
          stl::ascii res;
          res.info = std::string(sname.begin(), sname.end());
          res.triangles = std::move(triangles);

          return pure( std::move(res) );
        };
        
      };
    };

    auto parser = solid_def;

    const auto res = parser(in);
    
    if(!res) {
      throw std::runtime_error("parse error");
    } else {
      std::clog << "success" << std::endl;
    }

    // self = std::move(res.get());
    
    return in;
  }
  

  static std::istream& operator>>(std::istream& in, file& self) {
    if( is_binary(in) ) {
      binary b;
      in >> b;
      self = b;
    } else {
      ascii a;
      in >> a;
      self = a;
    }

    return in;
  };
  
}


int main(int argc, char** argv) {
  if(argc < 2) {
    std::cerr << "usage: " << argv[0] << " input.stl" << std::endl;
    return 1;
  }

  std::ifstream input(argv[1]);
  
  stl::file file;
  input >> file;

  // print first normal
  //std::cout << file.info << " " << n.x << " " << n.y << " " << n.z  << std::endl;
  
  return 0;
}
