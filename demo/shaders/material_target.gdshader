// Reference output for the Godot emitter (slang-emit-godot.cpp), hand-written.
//
// The generated shader for demo/shaders/material.slang is diffed against this file.
// Uniform names come from reflection, so they stay unmangled; locals, structs and
// functions carry Slang's `_N` suffixes, which is what the emitter passes through.

shader_type spatial;
render_mode cull_disabled;

uniform vec3 albedo_tint;
uniform vec4 rim_color : source_color;
uniform float rim_power : hint_range(0.0, 8.0);
uniform sampler2D albedo_map : source_color, hint_default_white, filter_linear_mipmap, repeat_enable;

struct Surface_0 {
	vec3 albedo_0;
	float alpha_0;
	float roughness_0;
	float metallic_0;
	vec3 emission_0;
};

Surface_0 shade_0(vec2 uv_0, vec3 normal_0, vec3 view_0) {
	Surface_0 s_0;
	s_0.albedo_0 = albedo_tint * texture(albedo_map, uv_0).xyz;
	s_0.alpha_0 = 1.0;
	s_0.roughness_0 = 0.4;
	s_0.metallic_0 = 0.0;
	float rim_0 = pow(1.0 - max(dot(normal_0, view_0), 0.0), rim_power);
	s_0.emission_0 = rim_color.xyz * rim_0;
	return s_0;
}

void fragment() {
	Surface_0 s_1 = shade_0(UV, NORMAL, VIEW);
	ALBEDO = s_1.albedo_0;
	ALPHA = s_1.alpha_0;
	ROUGHNESS = s_1.roughness_0;
	METALLIC = s_1.metallic_0;
	EMISSION = s_1.emission_0;
}
