import godot;

[gd::compositor::ColorTexture]
let out_color : RWTexture2D<float4>;

[gd::compositor::SceneBuffer("forward_clustered", "normal_roughness")]
let scene_texture : Texture2D<float4>;

[gd::LinearSampler(SamplerRepeatMode.REPEAT)]
let sampler_state : SamplerState;

[gd::compositor::Size]
let viewport_size : int2;

[shader("compute")]
[numthreads(8, 8, 1)]
func display_normals(uint3 threadId: SV_DispatchThreadID) {
	let coord = int2(threadId.xy);
	if (coord.x >= viewport_size.x || coord.y >= viewport_size.y) return;
	out_color[threadId.xy] = gd::normal_roughness_compatibility(scene_texture.Sample(sampler_state, float2(coord) / viewport_size));
}
