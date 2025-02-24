#include <cassert>
#include <chrono>
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <raylib.h>
#include <rlgl.h>

typedef struct ComputeShader
{
  Shader shader;
  void *fence;
} ComputeShader;

typedef struct ComputeBuffer
{
  unsigned int id;
  unsigned int pbo;
  unsigned int size;
} ComputeBuffer;

ComputeShader LoadComputeShader(const char *source)
{
  ComputeShader compute = {};

  unsigned int shader_id = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(shader_id, 1, &source, NULL);
  glCompileShader(shader_id);

  GLint success;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetShaderInfoLog(shader_id, 512, NULL, infoLog);
    TraceLog(LOG_ERROR, "Shader compilation failed: %s", infoLog);
    glDeleteShader(shader_id);
    return compute;
  }

  compute.shader.id = glCreateProgram();
  glAttachShader(compute.shader.id, shader_id);
  glLinkProgram(compute.shader.id);

  glGetProgramiv(compute.shader.id, GL_LINK_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetProgramInfoLog(compute.shader.id, 512, NULL, infoLog);
    TraceLog(LOG_ERROR, "Shader linking failed: %s", infoLog);
    glDeleteShader(shader_id);
    glDeleteProgram(compute.shader.id);
    return compute;
  }

  compute.shader.locs = (int *)RL_MALLOC(RL_MAX_SHADER_LOCATIONS * sizeof(int));
  for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; i++)
    compute.shader.locs[i] = -1;

  glDeleteShader(shader_id);
  return compute;
}

bool IsComputeShaderValid(ComputeShader shader)
{
  return IsShaderValid(shader.shader);
}

bool IsComputeBufferValid(ComputeBuffer buffer)
{
  return buffer.id > 0;
}

ComputeBuffer CreateComputeBuffer(void *data, int count)
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

  TraceLog(LOG_INFO, "Created compute buffer %d with size %d", buffer.id, count);

  return buffer;
}

void ComputeDispatch(ComputeShader *shader, ComputeBuffer buffer)
{
  assert(IsComputeShaderValid(*shader));

  glUseProgram(shader->shader.id);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer.id);
  const int groups = (buffer.size + 1023) / 1024;
  glDispatchCompute(groups, 1, 1);
  if (shader->fence)
    glDeleteSync((GLsync)shader->fence);
  shader->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  TraceLog(LOG_INFO, "Dispatched compute with %d groups\n", groups);
  glUseProgram(0);
}

bool IsComputeDone(ComputeShader shader)
{
  if (!IsComputeShaderValid(shader))
    return true;

  GLenum status = glClientWaitSync((GLsync)shader.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
  return (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED);
}

void UnloadComputeShader(ComputeShader shader)
{
  if (!IsComputeShaderValid(shader))
    return;

  UnloadShader(shader.shader);
  glDeleteSync((GLsync)shader.fence);
}

void UnloadComputeBuffer(ComputeBuffer buffer)
{
  glDeleteBuffers(1, &buffer.id);
  glDeleteBuffers(1, &buffer.pbo);
}

void CopyComputeBufferToTexture(ComputeBuffer buffer, unsigned int texture_id, int width, int height)
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

const char *cs_source = R"(
#version 430
layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 0) buffer Data {
    float colors[];
};
uniform float time;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    float t = time + float(idx) * 0.01;
    colors[idx * 4 + 0] = cos(time * 10.0 + t * 0.01) * 0.5 + 0.5;
    colors[idx * 4 + 1] = cos(time * 15.0 + t * 0.02) * 0.5 + 0.5;
    colors[idx * 4 + 2] = sin(time * 20.0 + t * 0.03) * 0.5 + 0.5;
    colors[idx * 4 + 3] = 1.0;
}
)";

int main()
{
  const int W           = 1440;
  const int H           = 900;
  const int pixel_count = W * H;

  InitWindow(W, H, "Compute Shader with Raylib");
  SetTargetFPS(0);

  if (glewInit() != GLEW_OK)
  {
    CloseWindow();
    return -1;
  }

  ComputeShader shader = LoadComputeShader(cs_source);
  assert(IsComputeShaderValid(shader));

  Image image          = GenImageColor(W, H, BLANK);
  ComputeBuffer buffer = CreateComputeBuffer(image.data, pixel_count * 4);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);
  Texture2D texture = LoadTextureFromImage(image);
  UnloadImage(image);

  float time = 0.0f;
  SetShaderValue(shader.shader, GetShaderLocation(shader.shader, "time"), &time, SHADER_UNIFORM_FLOAT);
  ComputeDispatch(&shader, buffer);

  uint64_t frame = 0;
  while (!WindowShouldClose())
  {
    time += GetFrameTime();

    auto start = std::chrono::high_resolution_clock::now();
    if (IsComputeDone(shader))
    {
      TraceLog(LOG_INFO, "Compute done");

      CopyComputeBufferToTexture(buffer, texture.id, W, H);

      SetShaderValue(shader.shader, GetShaderLocation(shader.shader, "time"), &time, SHADER_UNIFORM_FLOAT);
      ComputeDispatch(&shader, buffer);
    }
    auto end                  = std::chrono::high_resolution_clock::now();
    static auto max_poll_time = 0;

    auto poll_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    TraceLog(LOG_INFO, "Poll time: %d us", poll_time);
    if (frame > 30 && poll_time > max_poll_time)
      max_poll_time = poll_time;
    TraceLog(LOG_INFO, "Max poll time: %d us\n", max_poll_time);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    DrawFPS(10, 10);
    DrawText(TextFormat("Time: %.2f", time), 10, 30, 20, WHITE);
    static auto draw_poll_time = poll_time;
    if (frame % 60 == 0)
      draw_poll_time = poll_time;
    DrawText(TextFormat("Poll time: %d us", draw_poll_time), 10, 50, 20, WHITE);
    EndDrawing();

    frame++;
  }

  UnloadTexture(texture);
  UnloadComputeBuffer(buffer);
  UnloadComputeShader(shader);
  CloseWindow();

  return 0;
}
