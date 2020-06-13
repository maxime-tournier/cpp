#include "gl.hpp"

#include <QApplication>

#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>

#include "eigen.hpp"
#include <array>

using namespace eigen;

struct mesh {
  vector<vec3> vertices;
  vector<vec3> normals;
  vector<vec2> texcoords;
};


// static void callback(GLenum source, GLenum type, GLuint id, GLenum severity,
//                      GLsizei length, const GLchar* message,
//                      const void* userParam) {
//   std::fprintf(stderr,
//                "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
//                (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type,
//                severity, message);
// }


struct Viewer: QOpenGLWidget {
  // gl::vertex_attrib position;
  gl::geometry position, color;
  QOpenGLShaderProgram program;
  
  void initializeGL() override {
    static std::vector<std::array<GLfloat, 3>> positions = {
      {0, 0, 0},
      {1, 0, 0},
      {0, -1, 0},
      {0, 1, 0},
      {1, 0, 0},
      {0, 0, 0},      
    };


    program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                    "attribute highp vec4 vertex;\n"
                                    // "uniform highp mat4 matrix;\n"
                                    "void main(void)\n"
                                    "{\n"
                                    "   gl_Position = vertex;\n"
                                    "}");
    program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                    // "uniform mediump vec4 color;\n"
                                    "void main(void)\n"
                                    "{\n"
                                    "   gl_FragColor = vec4(1, 0, 0, 1);\n"
                                    "}");

    program.link();
    program.bind();
    
    position.attrib.index = program.attributeLocation("vertex");
    position.data(positions);
  }

  void resizeGL(int w, int h) override {
  }

  void paintGL() override {
    const auto lock = position.attrib.enable();
    glDrawArrays(GL_TRIANGLES, 0, 6);    
  }
  
};

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  Viewer widget;
  widget.show();
  return app.exec();
}
