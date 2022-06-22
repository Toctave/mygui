#pragma once

#include <GL/gl.h>

#define FOR_ALL_GL_FUNCTIONS(X)                                                \
    X(AttachShader, void, GLuint, GLuint)                                      \
    X(BindBuffer, void, GLenum, GLuint)                                        \
    X(BindVertexArray, void, GLuint)                                           \
    X(BufferData, void, GLenum, GLsizeiptr, const GLvoid*, GLenum)             \
    X(BufferSubData, void, GLenum, GLintptr, GLsizeiptr, const void*)          \
    X(CompileShader, void, GLuint)                                             \
    X(CreateProgram, GLuint)                                                   \
    X(CreateShader, GLuint, GLenum)                                            \
    X(DebugMessageCallback, void, GLDEBUGPROC, void*)                          \
    X(DeleteProgram, void, GLuint)                                             \
    X(DeleteShader, void, GLuint)                                              \
    X(EnableVertexAttribArray, void, GLuint)                                   \
    X(GenBuffers, void, GLsizei, GLuint*)                                      \
    X(GenVertexArrays, void, GLsizei, GLuint*)                                 \
    X(GetProgramInfoLog, void, GLuint, GLsizei, GLsizei*, GLchar*)             \
    X(GetProgramiv, void, GLuint, GLenum, GLint*)                              \
    X(GetShaderInfoLog, void, GLuint, GLsizei, GLsizei*, GLchar*)              \
    X(GetShaderiv, void, GLuint, GLenum, GLint*)                               \
    X(GetUniformLocation, GLint, GLuint, const GLchar*)                        \
    X(LinkProgram, void, GLuint)                                               \
    X(ShaderSource, void, GLuint, GLsizei, const GLchar**, const GLint*)       \
    X(Uniform1f, void, GLint, GLfloat)                                         \
    X(Uniform1fv, void, GLint, GLsizei, const GLfloat*)                        \
    X(Uniform1i, void, GLint, GLint)                                           \
    X(Uniform1iv, void, GLint, GLsizei, const GLint*)                          \
    X(Uniform1ui, void, GLuint, GLuint)                                        \
    X(Uniform1uiv, void, GLint, GLsizei, const GLuint*)                        \
    X(Uniform2f, void, GLint, GLfloat, GLfloat)                                \
    X(Uniform2fv, void, GLint, GLsizei, const GLfloat*)                        \
    X(Uniform2i, void, GLint, GLint, GLint)                                    \
    X(Uniform2iv, void, GLint, GLsizei, const GLint*)                          \
    X(Uniform2ui, void, GLuint, GLuint, GLuint)                                \
    X(Uniform2uiv, void, GLint, GLsizei, const GLuint*)                        \
    X(Uniform3f, void, GLint, GLfloat, GLfloat, GLfloat)                       \
    X(Uniform3fv, void, GLint, GLsizei, const GLfloat*)                        \
    X(Uniform3i, void, GLint, GLint, GLint, GLint)                             \
    X(Uniform3iv, void, GLint, GLsizei, const GLint*)                          \
    X(Uniform3ui, void, GLuint, GLuint, GLuint, GLuint)                        \
    X(Uniform3uiv, void, GLint, GLsizei, const GLuint*)                        \
    X(Uniform4f, void, GLint, GLfloat, GLfloat, GLfloat, GLfloat)              \
    X(Uniform4fv, void, GLint, GLsizei, const GLfloat*)                        \
    X(Uniform4i, void, GLint, GLint, GLint, GLint, GLint)                      \
    X(Uniform4iv, void, GLint, GLsizei, const GLint*)                          \
    X(Uniform4ui, void, GLuint, GLuint, GLuint, GLuint, GLuint)                \
    X(Uniform4uiv, void, GLint, GLsizei, const GLuint*)                        \
    X(UniformMatrix2fv, void, GLint, GLsizei, GLboolean, const GLfloat*)       \
    X(UniformMatrix2x3fv, void, GLint, GLsizei, GLboolean, const GLfloat*)     \
    X(UniformMatrix2x4fv, void, GLint, GLsizei, GLboolean, const GLfloat*)     \
    X(UniformMatrix3fv, void, GLint, GLsizei, GLboolean, const GLfloat*)       \
    X(UniformMatrix3x2fv, void, GLint, GLsizei, GLboolean, const GLfloat*)     \
    X(UniformMatrix3x4fv, void, GLint, GLsizei, GLboolean, const GLfloat*)     \
    X(UniformMatrix4fv, void, GLint, GLsizei, GLboolean, const GLfloat*)       \
    X(UniformMatrix4x2fv, void, GLint, GLsizei, GLboolean, const GLfloat*)     \
    X(UniformMatrix4x3fv, void, GLint, GLsizei, GLboolean, const GLfloat*)     \
    X(UseProgram, void, GLuint)                                                \
    X(VertexAttribPointer,                                                     \
      void,                                                                    \
      GLuint,                                                                  \
      GLint,                                                                   \
      GLenum,                                                                  \
      GLboolean,                                                               \
      GLsizei,                                                                 \
      const GLvoid*)

#define DO_DECLARE_EXTERN_GL_FUNCTION(name, rtype, ...)                        \
    extern rtype (*gl##name)(__VA_ARGS__);

FOR_ALL_GL_FUNCTIONS(DO_DECLARE_EXTERN_GL_FUNCTION)
