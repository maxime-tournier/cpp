#ifndef GL_HPP
#define GL_HPP

#include "finally.hpp"

#include <QApplication>
#include <QOpenGLWidget>

namespace gl {

  struct buffer {
    GLuint id = 0;

    void bind() {
      if(!id) {
        glGenBuffers(1, &id);
      }

      glBindBuffer(GL_ARRAY_BUFFER, id);
    }

    template<class T>
    void data(const T* ptr, std::size_t size, GLenum mode) {
      bind();
      glBufferData(GL_ARRAY_BUFFER, sizeof(T) * size, ptr, mode);
    }
    
  };


  struct vertex_attrib {
    GLuint index;
    GLuint size;
    GLenum type;
    GLenum normalize = false;
    GLuint stride = 0;
    std::size_t offset = 0;
    
    void pointer() {
      glVertexAttribPointer(size, size, type, normalize, stride, (void*) offset);
    }

    auto lock() const {
      glEnableVertexAttribArray(index);
      return finally([index=index]{ glDisableVertexAttribArray(index); });
    };
  };

  
}


#endif
