#include "compute.h"

#include <chrono>
#include <raylib.h>

#include <GL/glew.h>

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

  ComputeShader shader = load_compute_shader(cs_source);
  if (!is_compute_shader_valid(shader))
  {
    CloseWindow();
    return -1;
  }

  Image image          = GenImageColor(W, H, BLANK);
  ComputeBuffer buffer = create_compute_buffer(image.data, pixel_count * 4);
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);
  Texture2D texture = LoadTextureFromImage(image);
  UnloadImage(image);

  float time = 0.0f;
  set_shader_uniform_float(shader, "time", &time);
  compute_dispatch(&shader, buffer);

  uint64_t frame = 0;
  while (!WindowShouldClose())
  {
    time += GetFrameTime();

    auto start = std::chrono::high_resolution_clock::now();
    if (is_compute_done(shader))
    {
      copy_compute_buffer_to_texture(buffer, texture.id, W, H);

      set_shader_uniform_float(shader, "time", &time);
      compute_dispatch(&shader, buffer);
    }
    auto end                  = std::chrono::high_resolution_clock::now();
    static auto max_poll_time = 0;

    auto poll_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (frame > 30 && poll_time > max_poll_time)
      max_poll_time = poll_time;

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(texture, 0, 0, WHITE);
    DrawFPS(10, 10);
    DrawText(TextFormat("Time: %.2f", time), 10, 30, 20, WHITE);
    static auto draw_poll_time = poll_time;
    if (frame % 60 == 0)
      draw_poll_time = poll_time;
    DrawText(TextFormat("Poll time: %8d us (max: %8d us)", draw_poll_time, max_poll_time), 10, 50, 20, WHITE);
    EndDrawing();

    frame++;
  }

  UnloadTexture(texture);
  unload_compute_buffer(buffer);
  unload_compute_shader(shader);
  CloseWindow();

  return 0;
}
