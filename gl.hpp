#ifndef GL_HPP
#define GL_HPP

#include "finally.hpp"

#include <QApplication>
#include <QOpenGLWidget>

#include <iostream>

namespace gl {

  struct buffer {
    GLuint id = 0;

    void bind() {
      if(!id) {
        glGenBuffers(1, &id);
        assert(id);
      }

      glBindBuffer(GL_ARRAY_BUFFER, id);
    }

    template<class T>
    void data(const T* ptr, std::size_t size, GLenum mode) {
      glBufferData(GL_ARRAY_BUFFER, sizeof(T) * size, ptr, mode);
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
    if(err == GL_NO_ERROR) return;
    std::cerr << "error: " << err << std::endl;
    
    error_check();
  }

  struct vertex_attrib {
    GLuint index;

    void pointer(GLuint size, GLenum type, GLenum normalize = GL_FALSE,
                 GLuint stride = 0, std::size_t offset = 0) {
      glVertexAttribPointer(index, size, type, normalize, stride, (void*) offset);
    }

    auto enable() const {
      glEnableVertexAttribArray(index);
      return finally([index=index]{ glDisableVertexAttribArray(index); });
    };

    // convenience using traits
    template<class T>
    void pointer(GLenum normalize = GL_FALSE) {
      pointer(traits<T>::size, traits<T>::type, normalize);
    }
    
  };


  // buffer + attrib combo
  struct geometry {
    gl::buffer buffer;
    gl::vertex_attrib attrib;
    
    GLenum mode = GL_STATIC_DRAW;
    GLenum normalize = GL_FALSE;
    
    template<class Container>
    void data(const Container& container) {
      buffer.bind();
      buffer.data(container, mode);

      attrib.pointer<typename Container::value_type>(normalize);
    }
  };
  
}


#endif
