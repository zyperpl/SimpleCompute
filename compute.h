#ifndef SCOMPUTE_H
#define SCOMPUTE_H

#include <stdbool.h>

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

ComputeShader load_compute_shader(const char *source);
ComputeBuffer create_compute_buffer(void *data, int count);

bool is_compute_shader_valid(ComputeShader shader);
bool is_compute_buffer_valid(ComputeBuffer buffer);

void compute_dispatch(ComputeShader *shader, ComputeBuffer buffer);
bool is_compute_done(ComputeShader shader);

void copy_compute_buffer_to_texture(ComputeBuffer buffer, unsigned int texture_id, int width, int height);
void set_shader_uniform_float(ComputeShader shader, const char *name, float *value);

void unload_compute_shader(ComputeShader shader);
void unload_compute_buffer(ComputeBuffer buffer);

#if defined(__cplusplus)
}
#endif

#endif // SCOMPUTE_H
