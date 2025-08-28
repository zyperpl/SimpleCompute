#ifndef SCOMPUTE_H
#define SCOMPUTE_H

#include <stdbool.h>

#ifdef _WIN32
  #ifdef SCOMPUTE_BUILD_DLL
    #define SCEXPORT __declspec(dllexport)
  #elif defined(SCOMPUTE_USE_DLL)
    #define SCEXPORT __declspec(dllimport)
  #else
    #define SCEXPORT
  #endif
#else
  #ifdef SCOMPUTE_BUILD_SHARED
    #define SCEXPORT __attribute__((visibility("default")))
  #else
    #define SCEXPORT
  #endif
#endif

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

#if defined(__cplusplus)
extern "C"
{
#endif

SCEXPORT ComputeShader load_compute_shader(const char *source);
SCEXPORT ComputeBuffer create_compute_buffer(void *data, int count);

SCEXPORT bool is_compute_shader_valid(ComputeShader shader);
SCEXPORT bool is_compute_buffer_valid(ComputeBuffer buffer);

SCEXPORT void compute_dispatch(ComputeShader *shader, ComputeBuffer buffer);
SCEXPORT bool is_compute_done(ComputeShader shader);

SCEXPORT void copy_compute_buffer_to_texture(ComputeBuffer buffer, unsigned int texture_id, int width, int height);
SCEXPORT void set_shader_uniform_float(ComputeShader shader, const char *name, float *value);

SCEXPORT void unload_compute_shader(ComputeShader shader);
SCEXPORT void unload_compute_buffer(ComputeBuffer buffer);

#if defined(__cplusplus)
}
#endif

#endif // SCOMPUTE_H
