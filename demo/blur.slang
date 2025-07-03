import godot;

[gd::compositor::ScreenTexture]
RWTexture2D<float4> scene_color;

[gd::compositor::ScreenTexture]
Texture2D<float4> scene_texture;

[gd::LinearSampler]
SamplerState sampler_state;

static const int KERNEL_RADIUS = 2;
static const float kernel[5] = { 0.06136, 0.24477, 0.38774, 0.24477, 0.06136 };

[shader("compute")]
[numthreads(8, 8, 1)]
void blurHorizontal(uint3 threadId: SV_DispatchThreadID) {
    int2 texSize;
    scene_texture.GetDimensions(texSize.x, texSize.y);

    int2 coord = int2(threadId.xy);
    if (coord.x >= texSize.x || coord.y >= texSize.y) return;

    float4 color = float4(0, 0, 0, 0);

    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i) {
        int2 sampleCoord = coord + int2(i, 0);
        sampleCoord = clamp(sampleCoord, int2(0, 0), texSize - 1);
        color += scene_texture.SampleLevel(sampler_state, float2(sampleCoord) / texSize, 0) * kernel[i + KERNEL_RADIUS];
    }

    scene_color[coord] = color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void blurVertical(uint3 threadId: SV_DispatchThreadID) {
    int2 texSize;
    scene_texture.GetDimensions(texSize.x, texSize.y);

    int2 coord = int2(threadId.xy);
    if (coord.x >= texSize.x || coord.y >= texSize.y) return;

    float4 color = float4(0, 0, 0, 0);

    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i) {
        int2 sampleCoord = coord + int2(0, i);
        sampleCoord = clamp(sampleCoord, int2(0, 0), texSize - 1);
        color += scene_texture.SampleLevel(sampler_state, float2(sampleCoord) / texSize, 0) * kernel[i + KERNEL_RADIUS];
    }

    scene_color[coord] = color;
}
