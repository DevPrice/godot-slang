module godot;

__include "scene_data";

namespace gd {

public enum SamplerRepeatMode {
	REPEAT = 0,
	MIRRORED_REPEAT = 1,
	CLAMP_TO_EDGE = 2,
	CLAMP_TO_BORDER = 3,
	MIRROR_CLAMP_TO_EDGE = 4,
}

/**
  The raw normal and roughness buffer is stored in an optimized format, different than the one available in Spatial shaders.
  When sampling the buffer, a [conversion function](https://docs.godotengine.org/en/latest/classes/class_compositoreffect.html#class-compositoreffect-property-needs-normal-roughness) must be applied.
 */
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

/**
 Binds a parameter to the current time in seconds.
 Must be applied to a variable of type `float`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_TimeAttribute {};

/**
 Binds a parameter to the current frame number.
 Must be applied to a variable of type `int`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_FrameIdAttribute {};

/**
 Binds a parameter to a Godot [global shader parameter](https://godotengine.org/article/godot-40-gets-global-and-instance-shader-uniforms/#global-uniforms).
 Must be applied to a variable of type `float`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_GlobalParamAttribute {
	let name: string;
};

/**
 Binds a parameter a linear sampler with the provided repeat mode.
 Must be applied to a variable of type `SamplerState`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_LinearSamplerAttribute {
	let repeat_mode = SamplerRepeatMode.REPEAT;
};

/**
 Binds a parameter a nearest-neighbor sampler with the provided repeat mode.
 Must be applied to a variable of type `SamplerState`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_NearestSamplerAttribute {
    let repeat_mode = SamplerRepeatMode.REPEAT;
};

/**
 Binds a parameter to the scene data block uniform buffer.
 Must be applied to a ConstantBuffer of type `gd::compositor::SceneDataBlock`.
 Only available for compositor effects.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SceneDataAttribute {};

/**
 Binds a parameter to the internal size of the render buffer (size before upscaling) with which textures are created by default.
 Must be applied to a variable of type `int2`.
 Only available for compositor effects.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SizeAttribute {};

/**
 Binds a parameter to the color texture we are rendering 3D content to for the current view.
 Must be applied to a variable of type `Texture2D`.
 Only available for compositor effects.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_ColorTextureAttribute {};

/**
 Binds a parameter to the depth texture we are rendering 3D content to for the current view.
 Must be applied to a variable of type `Texture2D`.
 Only available for compositor effects.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_DepthTextureAttribute {};

/**
 Binds a parameter to the velocity texture we are rendering 3D content to for the current view.
 Must be applied to a variable of type `Texture2D`.
 Only available for compositor effects.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_VelocityTextureAttribute {};

/**
 Binds a parameter to the texture with the provided context and name.
 Must be applied to a variable of type `Texture2D`.
 Only available for compositor effects.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SceneBufferAttribute {
	let context: string = "render_buffers";
	let name: string;
};
