implementing godot;

#define MAX_VIEWS 2

#if GODOT_MAJOR_VERSION != 4
#error "This module requires Godot 4.x"
#endif

#if GODOT_MINOR_VERSION < 6
#define T_VIEW_MATRIX float4x4
#else
#define T_VIEW_MATRIX float3x4
#endif

namespace gd {

namespace compositor {

public struct SceneData {
	public float4x4 projection_matrix;
	public float4x4 inv_projection_matrix;
	public T_VIEW_MATRIX inv_view_matrix;
	public T_VIEW_MATRIX view_matrix;

	// only used for multiview
	public float4x4 projection_matrix_view[MAX_VIEWS];
	public float4x4 inv_projection_matrix_view[MAX_VIEWS];
	public float4 eye_offset[MAX_VIEWS];

	// Used for billboards to cast correct shadows.
	public float4x4 main_cam_inv_view_matrix;

	public float2 viewport_size;
	public float2 screen_pixel_size;

	// Use float4s because std140 doesn't play nice with float2s, z and w are wasted.
	public float4 directional_penumbra_shadow_kernel[32];
	public float4 directional_soft_shadow_kernel[32];
	public float4 penumbra_shadow_kernel[32];
	public float4 soft_shadow_kernel[32];

	public float2 shadow_atlas_pixel_size;
	public float2 directional_shadow_pixel_size;

#if GODOT_MINOR_VERSION >= 6
	public float radiance_pixel_size;
	public float radiance_border_size;
	public float2 reflection_atlas_border_size;
#endif

	public uint directional_light_count;
	public float dual_paraboloid_side;
	public float z_far;
	public float z_near;

	public float roughness_limiter_amount;
	public float roughness_limiter_limit;
	public float opaque_prepass_threshold;
	public uint flags;

	public float3x3 radiance_inverse_xform;

	public float4 ambient_light_color_energy;

	public float ambient_color_sky_mix;
	public float fog_density;
	public float fog_height;
	public float fog_height_density;

	public float fog_depth_curve;
	public float fog_depth_begin;
	public float fog_depth_end;
	public float fog_sun_scatter;

	public float3 fog_light_color;
	public float fog_aerial_perspective;

	public float time;
	public float taa_frame_count;
	public float2 taa_jitter;

	public float emissive_exposure_normalization;
	public float IBL_exposure_normalization;
	public uint camera_visible_layers;
	public float pass_alpha_multiplier;
};

public struct SceneDataBlock {
	public SceneData data;
	public SceneData prev_data;
};

}
}