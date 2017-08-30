// -*- compile-command: "c++ -std=c++11 stl.cpp -o stl -lstdc++" -*-

#include <stdint.h>
#include <iostream>
#include <vector>
#include <fstream>

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

  std::istream& operator>>(std::istream& in, vec3& self) {
    real32 tmp;
    for(float* ptr = &self.x; ptr < &self.x + 3; ++ptr) {
      in >> tmp;
      *ptr = tmp.value;
    }

    return in;
  }
  
  
  struct triangle {
    vec3 normal;
    vec3 vertex[3]; 
  };
  

  std::istream& operator>>(std::istream& in, triangle& self) {
    return in >> self.normal >> self.vertex[0] >> self.vertex[1] >> self.vertex[2];
  }

  
  union attribute_count {
    char bytes[2];
    std::uint16_t value;
  };

  std::istream& operator>>(std::istream& in, attribute_count& self) {
    return in.read(self.bytes, sizeof(self.bytes));
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


  // note: `in` is seeked
  static bool is_binary(std::istream& in) {
    
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
  
  
}


int main(int argc, char** argv) {
  if(argc < 2) {
    std::cerr << "usage: " << argv[0] << " input.stl" << std::endl;
    return 1;
  }

  std::ifstream input(argv[1], std::ios::in | std::ios::binary);
  
  stl::binary file;
  input >> file;

  // print first normal
  stl::vec3 n = file.triangles[0].normal;
  std::cout << file.info << " " << n.x << " " << n.y << " " << n.z  << std::endl;
  
  return 0;
}
