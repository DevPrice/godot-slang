import godot;

[gd::compositor::ScreenTexture]
RWTexture2D<float4> scene_color;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 threadId: SV_DispatchThreadID) {
    float4 color = scene_color[threadId.xy];
    scene_color[threadId.xy] = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
}
