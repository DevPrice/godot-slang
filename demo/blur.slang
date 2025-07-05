import godot;

[gd::compositor::ColorTexture]
RWTexture2D<float4> scene_color;

[gd::compositor::ColorTexture]
Texture2D<float4> scene_texture;

[gd::LinearSampler(SamplerRepeatMode.REPEAT)]
SamplerState sampler_state;

[gd::compositor::Size]
int2 viewport_size;

static const int KERNEL_RADIUS = 2;
static const float kernel[5] = { 0.06136, 0.24477, 0.38774, 0.24477, 0.06136 };

[shader("compute")]
[numthreads(8, 8, 1)]
void blurHorizontal(uint3 threadId: SV_DispatchThreadID) {
	let coord = int2(threadId.xy);
	if (coord.x >= viewport_size.x || coord.y >= viewport_size.y) return;

    var color = float4(0, 0, 0, 0);

    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i) {
		var sample_coord = coord + int2(i, 0);
		sample_coord = clamp(sample_coord, int2(0, 0), viewport_size - 1);
		color += scene_texture.SampleLevel(sampler_state, float2(sample_coord) / viewport_size, 0) * kernel[i + KERNEL_RADIUS];
    }

    scene_color[coord] = color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void blurVertical(uint3 threadId: SV_DispatchThreadID) {
	let coord = int2(threadId.xy);
	if (coord.x >= viewport_size.x || coord.y >= viewport_size.y) return;

    var color = float4(0, 0, 0, 0);

    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i) {
		var sample_coord = coord + int2(0, i);
		sample_coord = clamp(sample_coord, int2(0, 0), viewport_size - 1);
		color += scene_texture.SampleLevel(sampler_state, float2(sample_coord) / viewport_size, 0) * kernel[i + KERNEL_RADIUS];
    }

    scene_color[coord] = color;
}
