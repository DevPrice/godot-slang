// A minimal Godot material: one fragment entry point that fills in a surface.
//
// Unlike a compute kernel or a raster pass, this is compiled to Godot shading language rather
// than SPIR-V, so it can be assigned to a mesh and lit by Godot's own renderer.

import godot;
import material;

[gd::Export] float3 albedo_tint;
[gd::Color] float4 rim_color;
[gd::Export] float rim_power;

Sampler2D albedo_map;

[gd::Material("spatial")]
[shader("fragment")]
gd::Surface shade(gd::FragmentIn input) {
    gd::Surface surface = gd::Surface();
    surface.albedo = albedo_tint * albedo_map.Sample(input.uv).rgb;
    surface.roughness = 0.4;
    let rim = pow(1.0 - max(dot(input.normal, input.view), 0.0), rim_power);
    surface.emission = rim_color.rgb * rim;
    return surface;
}
