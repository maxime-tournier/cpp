#ifndef GL_HPP
#define GL_HPP

#include "finally.hpp"

#include <QApplication>
#include <QOpenGLWidget>

#include <iostream>

namespace gl {

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


  template<class Container>
  void data(const Container& container, GLenum mode) {
    data(container.data(), container.size(), mode);
  }
};

template<class T>
struct traits;

template<std::size_t N>
struct traits<std::array<GLfloat, N>> {
  static constexpr GLenum type = GL_FLOAT;
  static constexpr GLuint size = N;
  static_assert(N <= 4, "size error");
};

static void error_check() {
  GLenum err = glGetError();
  if(err == GL_NO_ERROR)
    return;
  std::cerr << "error: " << err << std::endl;

  error_check();
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

  auto enable(GLenum stride = 0, std::size_t offset = 0) {
    attrib_traits<kind>::enable(size, type, stride, (void*)offset);
    glEnableClientState(kind);
  }

  auto disable() { glDisableClientState(kind); };
};

struct geometry {
  
  template<GLenum kind>
  struct attrib_buffer: attrib<kind> {
    gl::buffer<GL_ARRAY_BUFFER> buffer;

    template<class Container>
    auto data(const Container& container, GLenum mode = GL_STATIC_DRAW) {
      buffer.bind().data(container, mode);
      using traits_type = gl::traits<typename Container::value_type>;
      this->size = traits_type::size;
      this->type = traits_type::type;
    }
  };

  attrib_buffer<GL_VERTEX_ARRAY> vertex;
  attrib_buffer<GL_NORMAL_ARRAY> normal;
  attrib_buffer<GL_COLOR_ARRAY> color;
  attrib_buffer<GL_TEXTURE_COORD_ARRAY> texcoord;

  template<class Cont>
  void foreach(Cont cont) {
    if(vertex.buffer.id) {
      cont(vertex);
    }

    if(normal.buffer.id) {
      cont(normal);
    }

    if(color.buffer.id) {
      cont(color);
    }

    if(texcoord.buffer.id) {
      cont(texcoord);
    }
  }

  auto enable() {
    foreach([](auto& attrib) {
      attrib.buffer.bind();
      attrib.enable();
    });
  }

  auto disable() {
    foreach([](auto& attrib) { attrib.disable(); });
  }

  auto use() {
    enable();
    return finally([this] { disable(); });
  }
};

} // namespace gl


#endif
