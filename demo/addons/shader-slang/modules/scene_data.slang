implementing godot;

namespace gd {

namespace compositor {

public struct SceneData {
    public highp float4x4 projection_matrix;
    public highp float4x4 inv_projection_matrix;
    public highp float4x4 inv_view_matrix;
    public highp float4x4 view_matrix;

    // only used for multiview
    public highp float4x4 projection_matrix_view[2];
    public highp float4x4 inv_projection_matrix_view[2];
    public highp float4 eye_offset[2];

    // Used for billboards to cast correct shadows.
    public highp float4x4 main_cam_inv_view_matrix;

    public highp float2 viewport_size;
    public highp float2 screen_pixel_size;

    // Use float4s because std140 doesn't play nice with float2s, z and w are wasted.
    public highp float4 directional_penumbra_shadow_kernel[32];
    public highp float4 directional_soft_shadow_kernel[32];
    public highp float4 penumbra_shadow_kernel[32];
	public highp float4 soft_shadow_kernel[32];

	public mediump float3x3 radiance_inverse_xform;

	public mediump float4 ambient_light_color_energy;

	public mediump float ambient_color_sky_mix;
	public bool use_ambient_light;
	public bool use_ambient_cubemap;
	public bool use_reflection_cubemap;

    public highp float2 shadow_atlas_pixel_size;
    public highp float2 directional_shadow_pixel_size;

    public uint directional_light_count;
    public mediump float dual_paraboloid_side;
    public highp float z_far;
    public highp float z_near;

    public bool roughness_limiter_enabled;
    public mediump float roughness_limiter_amount;
    public mediump float roughness_limiter_limit;
    public mediump float opaque_prepass_threshold;

    public bool fog_enabled;
    public uint fog_mode;
    public highp float fog_density;
    public highp float fog_height;
    public highp float fog_height_density;

    public highp float fog_depth_curve;
    public highp float pad;
    public highp float fog_depth_begin;

    public mediump float3 fog_light_color;
    public highp float fog_depth_end;

    public mediump float fog_sun_scatter;
    public mediump float fog_aerial_perspective;
    public highp float time;
    public mediump float reflection_multiplier; // one normally, zero when rendering reflections

    public float2 taa_jitter;
    public bool material_uv2_mode;
    public float emissive_exposure_normalization;

    public float IBL_exposure_normalization;
    public bool pancake_shadows;
    public uint camera_visible_layers;
    public float pass_alpha_multiplier;
};

public struct SceneDataBlock {
	public SceneData data;
	public SceneData prev_data;
};

}
}