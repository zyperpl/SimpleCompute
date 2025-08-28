#include "../compute.h"

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

float plasma(vec2 uv) {
    vec2 p = uv * 8.0;
    float wave1 = sin(p.x + time * 2.0);
    float wave2 = sin(p.y + time * 1.5);
    float wave3 = sin((p.x + p.y) * 0.7 + time * 3.0);
    float wave4 = sin(sqrt(p.x * p.x + p.y * p.y) + time * 2.5);
    return (wave1 + wave2 + wave3 + wave4) * 0.25;
}

vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    uint width = 1440;
    uint height = 900;
    uint total_pixels = width * height;
    
    if (idx >= total_pixels) return;
    
    uint x = idx % width;
    uint y = idx / width;
    
    vec2 uv = vec2(x, y) / vec2(width, height);
    uv = uv * 2.0 - 1.0;
    uv.x *= float(width) / float(height);
    
    float plasma_value = plasma(uv);
    vec3 color = palette(plasma_value + time * 0.5);
    
    float dist = length(uv);
    float ripple = sin(dist * 20.0 - time * 8.0) * 0.1;
    color += ripple;
    
    colors[idx * 4 + 0] = color.r;
    colors[idx * 4 + 1] = color.g;
    colors[idx * 4 + 2] = color.b;
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
