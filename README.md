# SimpleCompute

OpenGL compute shader library for real-time graphics.

![Screenshot](example/screenshot000.png)

## Build & Run

```bash
make all    # Build shared library
make run    # Run example
```

## Example

```glsl
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
```

## Usage

```c
ComputeShader shader = load_compute_shader(cs_source);
ComputeBuffer buffer = create_compute_buffer(data, size);
compute_dispatch(&shader, buffer);
```