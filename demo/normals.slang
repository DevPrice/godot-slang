import godot;

[gd::compositor::ScreenTexture]
let out_color : RWTexture2D<float4>;

[gd::compositor::SceneBuffer("forward_clustered", "normal_roughness")]
let scene_texture : Texture2D<float4>;

[gd::LinearSampler(SamplerRepeatMode.REPEAT)]
let sampler_state : SamplerState;

[shader("compute")]
[numthreads(8, 8, 1)]
func displayNormals(uint3 threadId: SV_DispatchThreadID) {
	var texSize : int2;
	scene_texture.GetDimensions(texSize.x, texSize.y);
	let color = out_color[threadId.xy];

	let coord = int2(threadId.xy);
	if (coord.x >= texSize.x || coord.y >= texSize.y) return;
	out_color[threadId.xy] = gd::normal_roughness_compatibility(scene_texture.Sample(sampler_state, float2(coord) / texSize));
}
