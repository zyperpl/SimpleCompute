#ifndef SCOMPUTE_H
#define SCOMPUTE_H

#include <cassert>
#include <stdio.h>

#define GLenum unsigned int
#define GLsync void *

extern "C"
{
  extern void glAttachShader(unsigned int program, unsigned int shader);
  extern void glBindBuffer(unsigned int target, unsigned int buffer);
  extern void glBindBufferBase(unsigned int target, unsigned int index, unsigned int buffer);
  extern void glBindTexture(GLenum target, unsigned int texture);
  extern void glBufferData(unsigned int target, int size, const void *data, unsigned int usage);
  extern unsigned int glClientWaitSync(void *sync, unsigned int flags, unsigned long long timeout);
  extern void glCompileShader(unsigned int shader);
  extern void glCopyBufferSubData(unsigned int readTarget,
                                  unsigned int writeTarget,
                                  int readOffset,
                                  int writeOffset,
                                  int size);
  extern unsigned int glCreateProgram();
  extern unsigned int glCreateShader(unsigned int type);
  extern void glDeleteBuffers(int n, const unsigned int *buffers);
  extern void glDeleteProgram(unsigned int program);
  extern void glDeleteShader(unsigned int shader);
  extern void glDeleteSync(void *sync);
  extern void glDispatchCompute(unsigned int num_groups_x, unsigned int num_groups_y, unsigned int num_groups_z);
  extern void *glFenceSync(unsigned int condition, unsigned int flags);
  extern void glGenBuffers(int n, unsigned int *buffers);
  extern void glGetProgramInfoLog(unsigned int program, int maxLength, int *length, char *infoLog);
  extern void glGetProgramiv(unsigned int program, unsigned int pname, int *params);
  extern void glGetShaderInfoLog(unsigned int shader, int maxLength, int *length, char *infoLog);
  extern void glGetShaderiv(unsigned int shader, unsigned int pname, int *params);
  extern int glGetUniformLocation(unsigned int program, const char *name);
  extern void glLinkProgram(unsigned int program);
  extern void glShaderSource(unsigned int shader, int count, const char **string, const int *length);
  extern void glTexSubImage2D(unsigned int target,
                              int level,
                              int xoffset,
                              int yoffset,
                              int width,
                              int height,
                              unsigned int format,
                              unsigned int type,
                              const void *pixels);
  extern void glUniform1f(int location, float v0);
  extern void glUseProgram(unsigned int program);
}

#define GL_COMPUTE_SHADER             0x91B9
#define GL_SHADER_STORAGE_BUFFER      0x90D2
#define GL_PIXEL_UNPACK_BUFFER        0x88EC
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_ALREADY_SIGNALED           0x911A
#define GL_CONDITION_SATISFIED        0x911C
#define GL_SYNC_FLUSH_COMMANDS_BIT    0x00000001
#define GL_RGBA                       0x1908
#define GL_FLOAT                      0x1406
#define GL_TEXTURE_2D                 0x0DE1
#define GL_TEXTURE0                   0x84C0
#define GL_COMPILE_STATUS             0x8B81
#define GL_LINK_STATUS                0x8B82
#define GL_STATIC_DRAW                0x88E4
#define GL_STREAM_DRAW                0x88E0

typedef struct ComputeShader
{
  int id;
  void *fence;
} ComputeShader;

typedef struct ComputeBuffer
{
  unsigned int id;
  unsigned int pbo;
  unsigned int size;
} ComputeBuffer;

#if defined(SC_USE_LOG)
#define SCLOG(...)       \
  do                     \
  {                      \
    printf("[SC] ");     \
    printf(__VA_ARGS__); \
  } while (false)
#else
#define SCLOG(...)
#endif

ComputeShader load_compute_shader(const char *source)
{
  ComputeShader compute = {};

  unsigned int shader_id = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(shader_id, 1, &source, NULL);
  glCompileShader(shader_id);

  int success;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetShaderInfoLog(shader_id, 512, NULL, infoLog);
    SCLOG("Shader compilation failed: %s", infoLog);
    glDeleteShader(shader_id);
    return compute;
  }

  compute.id = glCreateProgram();
  glAttachShader(compute.id, shader_id);
  glLinkProgram(compute.id);

  glGetProgramiv(compute.id, GL_LINK_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetProgramInfoLog(compute.id, 512, NULL, infoLog);
    SCLOG("Shader linking failed: %s", infoLog);
    glDeleteShader(shader_id);
    glDeleteProgram(compute.id);
    return compute;
  }

  glDeleteShader(shader_id);
  return compute;
}

bool is_compute_shader_valid(ComputeShader shader)
{
  return shader.id > 0;
}

bool is_compute_buffer_valid(ComputeBuffer buffer)
{
  return buffer.id > 0;
}

ComputeBuffer create_compute_buffer(void *data, int count)
{
  ComputeBuffer buffer = { 0 };
  glGenBuffers(1, &buffer.id);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.id);
  glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(float), data, GL_STATIC_DRAW);
  buffer.size = count * sizeof(float);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  glGenBuffers(1, &buffer.pbo);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.pbo);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, count * sizeof(float), NULL, GL_STREAM_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  SCLOG("Created compute buffer %d with size %d", buffer.id, count);

  return buffer;
}

void compute_dispatch(ComputeShader *shader, ComputeBuffer buffer)
{
  assert(is_compute_shader_valid(*shader));

  glUseProgram(shader->id);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer.id);
  const int groups = (buffer.size + 1023) / 1024;
  glDispatchCompute(groups, 1, 1);
  if (shader->fence)
    glDeleteSync((GLsync)shader->fence);
  shader->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  SCLOG("Dispatched compute with %d groups\n", groups);
  glUseProgram(0);
}

bool is_compute_done(ComputeShader shader)
{
  if (!is_compute_shader_valid(shader))
    return true;

  GLenum status = glClientWaitSync((GLsync)shader.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
  return (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED);
}

void unload_compute_shader(ComputeShader shader)
{
  if (!is_compute_shader_valid(shader))
    return;

  glDeleteProgram(shader.id);
  if (shader.fence)
    glDeleteSync((GLsync)shader.fence);
}

void unload_compute_buffer(ComputeBuffer buffer)
{
  glDeleteBuffers(1, &buffer.id);
  glDeleteBuffers(1, &buffer.pbo);
}

void copy_compute_buffer_to_texture(ComputeBuffer buffer, unsigned int texture_id, int width, int height)
{
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.id);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.pbo);
  glCopyBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_PIXEL_UNPACK_BUFFER, 0, 0, buffer.size);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, 0);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void set_shader_uniform_float(ComputeShader shader, const char *name, float *value)
{
  glUseProgram(shader.id);
  glUniform1f(glGetUniformLocation(shader.id, name), *value);
}

#undef GLenum
#undef GLsync

#undef GL_COMPUTE_SHADER
#undef GL_SHADER_STORAGE_BUFFER
#undef GL_PIXEL_UNPACK_BUFFER
#undef GL_SYNC_GPU_COMMANDS_COMPLETE
#undef GL_ALREADY_SIGNALED
#undef GL_CONDITION_SATISFIED
#undef GL_SYNC_FLUSH_COMMANDS_BIT
#undef GL_RGBA
#undef GL_FLOAT
#undef GL_TEXTURE_2D
#undef GL_TEXTURE0
#undef GL_COMPILE_STATUS
#undef GL_LINK_STATUS
#undef GL_STATIC_DRAW
#undef GL_STREAM_DRAW

#endif // SCOMPUTE_H
