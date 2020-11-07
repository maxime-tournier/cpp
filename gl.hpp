#ifndef GL_HPP
#define GL_HPP

#include <GL/glew.h>

#include "finally.hpp"

#include <array>
#include <cassert>

#include <iostream>

namespace gl {


static void init() {
  GLenum err = glewInit();
  if(GLEW_OK != err) {
    throw std::runtime_error((const char*)glewGetErrorString(err));
  }
}

  

template<GLenum kind>
struct buffer {
  GLuint id = 0;

  buffer& bind() {
    if(!id) {
      glGenBuffers(1, &id);
      assert(id);
    }

    glBindBuffer(kind, id);
    return *this;
  }

  template<class T>
  void data(const T* ptr, std::size_t size, GLenum mode) {
    glBufferData(kind, sizeof(T) * size, ptr, mode);
  }


  // convenience
  template<class Container>
  void data(const Container& container, GLenum mode) {
    data(container.data(), container.size(), mode);
  }
};


template<class T>
struct value_traits;

template<std::size_t N>
struct value_traits<std::array<GLfloat, N>> {
  static constexpr GLenum type = GL_FLOAT;
  static constexpr GLuint size = N;
  static_assert(N <= 4, "size error");
};

static bool error_check(std::ostream& out=std::cerr) {
  GLenum err = glGetError();
  if(err == GL_NO_ERROR) {
    return false;
  }

  out << "error: " << err << "\n";
  
  return error_check(out) || true;
}

// old-school vertex/color/normal pointers
template<GLenum kind>
struct attrib_traits;

template<>
struct attrib_traits<GL_VERTEX_ARRAY> {
  template<class... Args>
  static void enable(Args... args) {
    return glVertexPointer(args...);
  }
};

template<>
struct attrib_traits<GL_COLOR_ARRAY> {
  template<class... Args>
  static void enable(Args... args) {
    return glColorPointer(args...);
  }
};

template<>
struct attrib_traits<GL_NORMAL_ARRAY> {
  template<class... Args>
  static void enable(GLuint, Args... args) {
    return glNormalPointer(args...);
  }
};

template<>
struct attrib_traits<GL_TEXTURE_COORD_ARRAY> {
  template<class... Args>
  static void enable(Args... args) {
    return glTexCoordPointer(args...);
  }
};

template<GLenum kind>
struct attrib {
  GLuint size;
  GLuint type;

  auto enable(GLenum stride = 0, std::size_t offset = 0) const {
    glEnableClientState(kind);
    attrib_traits<kind>::enable(size, type, stride, (void*)offset);
  }

  auto disable() const { glDisableClientState(kind); };
};

struct geometry {
  
  template<GLenum kind>
  struct attrib_buffer: attrib<kind> {
    gl::buffer<GL_ARRAY_BUFFER> buffer;

    template<class Container>
    auto data(const Container& container, GLenum mode = GL_STATIC_DRAW) {
      buffer.bind().data(container, mode);
      using traits_type = gl::value_traits<typename Container::value_type>;
      this->size = traits_type::size;
      this->type = traits_type::type;
    }
  };

  attrib_buffer<GL_VERTEX_ARRAY> vertex;
  attrib_buffer<GL_NORMAL_ARRAY> normal;
  attrib_buffer<GL_COLOR_ARRAY> color;
  attrib_buffer<GL_TEXTURE_COORD_ARRAY> texcoord;

  template<class Self, class Cont>
  friend void foreach(Self&& self, Cont cont) {
    if(self.vertex.buffer.id) {
      cont(self.vertex);
    }

    if(self.normal.buffer.id) {
      cont(self.normal);
    }

    if(self.color.buffer.id) {
      cont(self.color);
    }

    if(self.texcoord.buffer.id) {
      cont(self.texcoord);
    }
  }

  auto enable() {
    foreach(*this, [](auto& attrib) {
      attrib.buffer.bind();
      attrib.enable();
    });
  }

  auto disable() const {
    foreach(*this, [](auto& attrib) { attrib.disable(); });
  }

  auto lock() {
    enable();
    return finally([this] { disable(); });
  }
};

  
} // namespace gl


#endif
