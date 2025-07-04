module godot;

namespace gd {

public enum SamplerRepeatMode {
	REPEAT = 0,
	MIRRORED_REPEAT = 1,
	CLAMP_TO_EDGE = 2,
	CLAMP_TO_BORDER = 3,
	MIRROR_CLAMP_TO_EDGE = 4,
}

public func normal_roughness_compatibility(float4 normal_roughness) -> float4 {
	var roughness = normal_roughness.w;
	if (roughness > 0.5) {
		roughness = 1.0 - roughness;
	}
	roughness /= (127.0 / 255.0);
	return float4(normalize(normal_roughness.xyz * 2.0 - 1.0) * 0.5 + 0.5, roughness);
}

}

using gd;

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_LinearSamplerAttribute {
	let repeat_mode = SamplerRepeatMode.REPEAT;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_NearestSamplerAttribute {
	let repeat_mode = SamplerRepeatMode.REPEAT;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_ScreenTextureAttribute {};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_DepthTextureAttribute {};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_VelocityTextureAttribute {};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SceneBufferAttribute {
	let context: string = "render_buffers";
	let name: string;
};
