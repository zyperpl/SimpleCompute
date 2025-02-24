#include <GL/glew.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <raylib.h>
#include <thread>

typedef struct ComputeShader
{
  unsigned int id;
  void *compute_fence;
} ComputeShader;

typedef struct ComputeBuffer
{
  unsigned int bufferId;
  unsigned int pbo;
  unsigned int size;
} ComputeBuffer;

ComputeShader LoadComputeShader(const char *source)
{
  ComputeShader shader = { 0 };

  unsigned int shader_id = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(shader_id, 1, &source, NULL);
  glCompileShader(shader_id);

  GLint success;
  glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetShaderInfoLog(shader_id, 512, NULL, infoLog);
    std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    glDeleteShader(shader_id);
    return shader;
  }

  shader.id = glCreateProgram();
  glAttachShader(shader.id, shader_id);
  glLinkProgram(shader.id);

  glGetProgramiv(shader.id, GL_LINK_STATUS, &success);
  if (!success)
  {
    char infoLog[512];
    glGetProgramInfoLog(shader.id, 512, NULL, infoLog);
    std::cerr << "Program linking failed: " << infoLog << std::endl;
    glDeleteShader(shader_id);
    glDeleteProgram(shader.id);
    return shader;
  }

  glDeleteShader(shader_id);
  return shader;
}

bool IsComputeShaderValid(ComputeShader shader)
{
  return shader.id > 0;
}

bool IsComputeBufferValid(ComputeBuffer buffer)
{
  return buffer.bufferId > 0;
}

ComputeBuffer ComputeBufferCreate(void *data, int count)
{
  ComputeBuffer buffer = { 0 };
  glGenBuffers(1, &buffer.bufferId);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.bufferId);
  glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(float), data, GL_STATIC_DRAW);
  buffer.size = count * sizeof(float);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  glGenBuffers(1, &buffer.pbo);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer.pbo);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, count * sizeof(float), NULL, GL_STREAM_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  return buffer;
}

void ComputeDispatch(ComputeShader shader, ComputeBuffer buffer, int groups = 4)
{
  if (!IsComputeShaderValid(shader))
    return;

  glUseProgram(shader.id);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer.bufferId);
  glDispatchCompute(groups, 1, 1);
  if (shader.compute_fence)
    glDeleteSync((GLsync)shader.compute_fence);
  shader.compute_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  printf("Dispatched compute with %d groups\n", groups);
  glUseProgram(0);
}

bool IsComputeDone(ComputeShader shader)
{
  if (!shader.compute_fence)
    return true;
  GLenum status = glClientWaitSync((GLsync)shader.compute_fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
  return (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED);
}

void ComputeBufferWait(ComputeBuffer buffer)
{
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.bufferId);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void UnloadComputeShader(ComputeShader shader)
{
  if (!IsComputeShaderValid(shader))
    return;

  glDeleteProgram(shader.id);
  if (shader.compute_fence)
    glDeleteSync((GLsync)shader.compute_fence);
}

void UnloadComputeBuffer(ComputeBuffer buffer)
{
  glDeleteBuffers(1, &buffer.bufferId);
  glDeleteBuffers(1, &buffer.pbo);
}

void SetComputeShaderValue(ComputeShader shader, const char *name, float value)
{
  if (!IsComputeShaderValid(shader))
    return;

  glUseProgram(shader.id);
  int loc = glGetUniformLocation(shader.id, name);
  glUniform1f(loc, value);
}

void CopyComputeBufferToTexture(ComputeBuffer buffer, unsigned int texture_id, int width, int height)
{
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.bufferId);
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
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Data {
    float colors[];
};
uniform float time;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    float t = time + float(idx) * 0.01;
    colors[idx * 4 + 0] = sin(time * 10.0 + t * 0.01) * 0.5 + 0.5;
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
  ComputeBuffer buffer = ComputeBufferCreate(image.data, pixel_count * 4);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);
  Texture2D texture = LoadTextureFromImage(image);
  UnloadImage(image);

  float time = 0.0f;
  SetComputeShaderValue(shader, "time", time);
  ComputeDispatch(shader, buffer, pixel_count / 256);

  uint64_t frame = 0;
  while (!WindowShouldClose())
  {
    time += GetFrameTime();

    auto start = std::chrono::high_resolution_clock::now();
    if (IsComputeDone(shader))
    {
      std::cout << " Done!" << std::endl;

      CopyComputeBufferToTexture(buffer, texture.id, W, H);

      SetComputeShaderValue(shader, "time", time);
      ComputeDispatch(shader, buffer, pixel_count / 256);
    }
    else
    {
      std::cout << ".";
    }
    auto end                  = std::chrono::high_resolution_clock::now();
    static auto max_poll_time = 0;

    auto poll_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Poll time: " << poll_time << "us" << std::endl;
    if (frame > 30 && poll_time > max_poll_time)
      max_poll_time = poll_time;
    printf("Max poll time: %d us\n", max_poll_time);

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
