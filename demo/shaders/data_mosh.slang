import godot;

using gd;
using gd::compositor;

[gd::compositor::SceneData]
let scene : ConstantBuffer<gd::compositor::SceneDataBlock>;

[gd::compositor::Texture(DataFormat.DATA_FORMAT_R16G16B16A16_SNORM)]
let capture : Texture2D<float4>;

[gd::compositor::Texture(DataFormat.DATA_FORMAT_R16G16B16A16_SNORM)]
[format("rgba16f")]
[gd::compositor::TextureName("scene_color")]
let out_capture : RWTexture2D<float4>;

[gd::compositor::VelocityTexture]
let velocity_texture : Texture2D<float4>;

[gd::compositor::DepthTexture]
let scene_depth : Texture2D<float4>;

[gd::LinearSampler(SamplerRepeatMode.REPEAT)]
let sampler_state : SamplerState;

[gd::compositor::ColorTexture]
let out_image : RWTexture2D<float4>;

func derive_motion_vector(float2 uv, float depth, float4x4 reprojection_matrix) -> float2{
	float4 previous_pos_ndc = mul(reprojection_matrix, float4(uv * 2.0f - 1.0f, depth * 2.0f - 1.0f, 1.0f));
	return 0.5f + (previous_pos_ndc.xy / previous_pos_ndc.w) * 0.5f - uv;
}

func motion_vector(float2 uv, float depth, float4x4 reprojection_matrix) -> float2{
	float2 velocity = velocity_texture.Sample(sampler_state, uv).xy;
	if (velocity.x == -1.f && velocity.y == -1.f) {
		// TODO: Fix this
		// return derive_motion_vector(uv, depth, reprojection_matrix);
		return float2(0.f);
	}
    return velocity;
}

[shader("compute")]
[numthreads(8, 8, 1)]
[gd::compositor::Once]
func capture_screen(uint3 thread_id: SV_DispatchThreadID) -> void {
    int2 xy = int2(thread_id.xy);
    if (xy.x > scene.data.viewport_size.x || xy.y > scene.data.viewport_size.y) {
        return;
    }
    out_capture[xy] = out_image[xy];
}

[shader("compute")]
[numthreads(8, 8, 1)]
func data_mosh(uint3 thread_id: SV_DispatchThreadID) -> void {
    int2 xy = int2(thread_id.xy);
	if (xy.x > scene.data.viewport_size.x || xy.y > scene.data.viewport_size.y) {
		return;
	}

	float2 screen_uv = float2(xy + .5f) / scene.data.viewport_size;

	float depth = scene_depth.Sample(sampler_state, screen_uv).r;
	float2 velocity = motion_vector(screen_uv, depth, scene.data.inv_projection_matrix);

    float3 out_color = capture.Sample(sampler_state, screen_uv + velocity).rgb;

    out_image[xy] = float4(out_color, 1.f);
    out_capture[xy] = out_image[xy];
}
