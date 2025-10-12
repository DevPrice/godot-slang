#version 450

import godot;

layout(local_size_x = 8, local_size_y = 8) in;

[gd::compositor::ColorTexture]
layout(rgba32f, binding = 0) uniform image2D scene_color;

[shader("compute")]
[numthreads(8, 8, 1)]
void main() {
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);

    vec4 color = imageLoad(scene_color, threadId);
    float gray = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
    imageStore(scene_color, threadId, vec4(gray, gray, gray, color.a));
}
