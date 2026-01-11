import godot;

using gd;
using gd::compositor;

[gd::compositor::InternalSize]
let viewport_size : int2;

[gd::compositor::Texture(DataFormat.R16G16B16A16_SNORM)]
[gd::LinearSampler(SamplerRepeatMode.CLAMP_TO_EDGE)]
let capture : Sampler2D<float4>;

[gd::compositor::TextureName("capture")]
let out_capture : RWTexture2D<float4>;

[gd::compositor::VelocityTexture]
let velocity_texture : Sampler2D<float4>;

[gd::compositor::ColorTexture]
let out_image : RWTexture2D<float4>;

func motion_vector(float2 uv) -> float2 {
    let velocity = velocity_texture.Sample(uv).xy;
	if (velocity.x == -1.f && velocity.y == -1.f) {
		return float2(0.f);
	}
    return velocity;
}

[shader("compute")]
[numthreads(8, 8, 1)]
[gd::compositor::Once]
func capture_screen(uint3 thread_id: SV_DispatchThreadID) -> void {
    let xy = int2(thread_id.xy);
    if (xy.x > viewport_size.x || xy.y > viewport_size.y) {
        return;
    }
    out_capture[xy] = out_image[xy];
}

[shader("compute")]
[numthreads(8, 8, 1)]
func data_mosh(uint3 thread_id: SV_DispatchThreadID) -> void {
    let xy = int2(thread_id.xy);
    if (xy.x > viewport_size.x || xy.y > viewport_size.y) {
		return;
	}

    let screen_uv = (xy + .5f) / viewport_size;
    let velocity = motion_vector(screen_uv);
    let out_color = capture.Sample(screen_uv + velocity).rgb;

    out_image[xy] = float4(out_color, 1.f);
    out_capture[xy] = out_image[xy];
}
