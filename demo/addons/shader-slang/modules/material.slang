implementing godot;

namespace gd {

/**
 The fragment-stage inputs Godot provides to a `shader_type spatial` material.
 Field names map to Godot's built-ins; the emitter binds them by name, so a field
 the shader never reads costs nothing.

 Spaces follow Godot, not Slang: `vertex`, `normal`, `tangent` and `binormal` are
 in view space, and `view` points from the fragment toward the camera.
*/
public struct FragmentIn {
    /// Position of the fragment in view space.
    public float3 vertex : GD_VERTEX;
    /// Interpolated normal in view space.
    public float3 normal : GD_NORMAL;
    /// Interpolated tangent in view space.
    public float3 tangent : GD_TANGENT;
    /// Interpolated binormal in view space.
    public float3 binormal : GD_BINORMAL;
    /// Primary texture coordinate.
    public float2 uv : GD_UV;
    /// Secondary texture coordinate.
    public float2 uv2 : GD_UV2;
    /// Interpolated vertex color.
    public float4 color : GD_COLOR;
    /// Unit vector from the fragment toward the camera.
    public float3 view : GD_VIEW;
    /// Position of the fragment in normalized screen coordinates.
    public float2 screen_uv : GD_SCREEN_UV;
    /// Fragment coordinate in pixels, with depth in `w`.
    public float4 frag_coord : GD_FRAGCOORD;
    /// True when rendering the front face of the primitive.
    public bool front_facing : GD_FRONT_FACING;
}

/**
 The surface properties a `shader_type spatial` material writes.

 Every field is assigned to its Godot built-in, so the defaults here have to match
 Godot's own: a field left alone must leave the built-in as Godot found it. Construct
 with `Surface s = Surface();` to start from those defaults.
*/
public struct Surface {
    /// Base color, before lighting.
    public float3 albedo : GD_ALBEDO;
    /// Opacity. Writing anything but 1.0 makes the material transparent.
    public float alpha : GD_ALPHA;
    /// Dielectric-to-metal blend.
    public float metallic : GD_METALLIC;
    /// Specular reflectance for dielectrics.
    public float specular : GD_SPECULAR;
    /// Surface microfacet roughness.
    public float roughness : GD_ROUGHNESS;
    /// Light emitted by the surface, unaffected by lighting.
    public float3 emission : GD_EMISSION;
    /// Tangent-space normal. Leave at (0.5, 0.5, 1.0) for no perturbation.
    public float3 normal_map : GD_NORMAL_MAP;
    /// Strength of `normal_map`.
    public float normal_map_depth : GD_NORMAL_MAP_DEPTH;
    /// Ambient occlusion.
    public float ao : GD_AO;
    /// How much `ao` affects direct light.
    public float ao_light_affect : GD_AO_LIGHT_AFFECT;
    /// Rim lighting strength.
    public float rim : GD_RIM;
    /// Blend between white and albedo for rim lighting.
    public float rim_tint : GD_RIM_TINT;

    public __init() {
        albedo = float3(1.0);
        alpha = 1.0;
        metallic = 0.0;
        specular = 0.5;
        roughness = 1.0;
        emission = float3(0.0);
        normal_map = float3(0.5, 0.5, 1.0);
        normal_map_depth = 1.0;
        ao = 1.0;
        ao_light_affect = 0.0;
        rim = 0.0;
        rim_tint = 0.5;
    }
}

}

/**
 Marks an entry point as a Godot material and selects the shader type.

 The shader type is emitted verbatim as Godot's `shader_type`, so it must be one Godot
 accepts. Only "spatial" is supported today.
*/
[__AttributeUsage(_AttributeTargets.Function)]
public struct gd_MaterialAttribute {
    let shader_type : String = "spatial";
};

/**
 Sets the `render_mode` line of the generated shader.

 The value is emitted verbatim after `render_mode`, so a material wanting more than one
 mode separates them with commas exactly as Godot does:
 `[gd::material::RenderMode("cull_disabled, depth_draw_opaque")]`.
*/
[__AttributeUsage(_AttributeTargets.Function)]
public struct gd_material_RenderModeAttribute {
    let render_mode : String;
};
