import godot;

[gd::compositor::ColorTexture]
let out_color : RWTexture2D<float4>;

[gd::compositor::ColorTexture]
let color_texture : Texture2D<float4>;

[gd::compositor::DepthTexture]
let depth_texture : Texture2D<float4>;

[gd::compositor::SceneBuffer("forward_clustered", "normal_roughness")]
let normal_roughness_texture : Texture2D<float4>;

[gd::LinearSampler(SamplerRepeatMode.REPEAT)]
let sampler_state : SamplerState;

[gd::compositor::InternalSize]
let viewport_size : int2;

[gd::compositor::SceneData]
let scene_data : ConstantBuffer<gd::compositor::SceneDataBlock>;

static let depth_sensitivity : float = 0.25;
static let normal_sensitivity : float = 0.1;

struct PushConstants {
    let outline_size : float = 1.0;
    [gd::Color]
    let outline_color : float4;
}

[vk::push_constant]
let push_constants : PushConstants;

[shader("compute")]
[numthreads(8, 8, 1)]
func outline(uint3 threadId: SV_DispatchThreadID) {
    let coord = int2(threadId.xy);
    if (coord.x >= viewport_size.x || coord.y >= viewport_size.y) return;

    let screen_uv = float2(coord + .5f) / viewport_size;

    let edge_strength = edge(screen_uv, viewport_size, scene_data.data.inv_projection_matrix, push_constants.outline_size);
    let base_color = color_texture.Sample(sampler_state, screen_uv);
    let edge_color = lerp(base_color.rgb, push_constants.outline_color.rgb, push_constants.outline_color.a);
    let result = lerp(base_color.rgb, edge_color, edge_strength);

    out_color[threadId.xy] = float4(result, base_color.a);
}

float edge(float2 uv, float2 viewport_size, float4x4 inv_projection, float scale) {
    let pixel_size = scale / viewport_size;

    float2 offsets[9] = {
        float2(-1, -1), float2(-1, 0), float2(-1, 1),
        float2(0, -1), float2(0, 0), float2(0, 1),
        float2(1, -1), float2(1, 0), float2(1, 1)
    };

    float depth_kernel[9] = {
        -0.5f, -1.5f, -0.5f,
        -1.5f, 8.0f, -1.5f,
        -0.5f, -1.5f, -0.5f
    };

    float normal_kernel[9] = {
        -1.f, -1.f, -1.f,
        -1.f, 8.f, -1.f,
        -1.f, -1.f, -1.f
    };

    var depth_sum = 0.0f;
    var normal_sum = float3(0.0f);

    for (int i = 0; i < 9; ++i) {
        let sample_uv = uv + pixel_size * offsets[i];

        let depth = log(linear_depth(sample_uv, inv_projection) + 2);
        depth_sum += depth * depth_kernel[i];

        let normal = gd::normal_roughness_compatibility(normal_roughness_texture.Sample(sampler_state, sample_uv)).xyz;
        normal_sum += normal * normal_kernel[i];
    }

    let depth_edge_val = step(depth_sensitivity * depth_sensitivity, abs(depth_sum));
    let normal_edge_val = step(normal_sensitivity, length(normal_sum));

    return max(depth_edge_val, normal_edge_val);
}

float linear_depth(float2 uv, float4x4 inv_projection) {
    let depth = depth_texture.Sample(sampler_state, uv).r;
    let ndc = float3(uv * 2.0 - 1.0, depth);
    var view = mul(float4(ndc, 1.0), inv_projection);
    view.xyz /= view.w;
    return log(-view.z);
}
