module godot;

__include "scene_data";
__include "data_format";

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
 Marks an attribute as an autobinding, which means that the user should not set it manually and it will be readonly in the property inspector.
*/
[__AttributeUsage(_AttributeTargets.Struct)]
public struct gd_AutobindAttribute {};

/**
 Indicates that a struct is represented by a global Godot class.
*/
[__AttributeUsage(_AttributeTargets.Struct)]
public struct gd_ClassAttribute {
    let class_name : String;
};

/**
 Indicates that a variable uses the specified name within Godot.
 The specified name will be emitted in the reflection metadata instead of the name declared in the shader code.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_NameAttribute {
    let name : String;
};

/**
 Hides a parameter from the property inspector. Use for parameters that should only be set from code.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_HideAttribute {};

/**
 Binds a parameter to the current time in seconds.
 Must be applied to a variable of type `float`.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_TimeAttribute {};

/**
 Binds a parameter to the current frame number.
 Must be applied to a variable of type `int`.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_FrameIdAttribute {};

/**
 Binds a parameter to the current mouse position.
 Must be applied to a variable of type `int2`.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_MousePositionAttribute {};

/**
 Will bind a white color/texture if no texture is bound by the user.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_DefaultWhiteAttribute {};

/**
 Binds a parameter to a Godot [global shader parameter](https://godotengine.org/article/godot-40-gets-global-and-instance-shader-uniforms/#global-uniforms).
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_GlobalParamAttribute {
    let name: string;
};

/**
 Binds a parameter a linear sampler with the provided repeat mode.
 Must be applied to a variable of type `SamplerState` or `Sampler2D`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_LinearSamplerAttribute {
    let repeat_mode = SamplerRepeatMode.REPEAT;
};

/**
 Binds a parameter a nearest-neighbor sampler with the provided repeat mode.
 Must be applied to a variable of type `SamplerState` or `Sampler2D`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_NearestSamplerAttribute {
    let repeat_mode = SamplerRepeatMode.REPEAT;
};

/**
 Indicates that a vector represents a color.
 Must be applied to a variable of type `float3` or `float4`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_ColorAttribute {};

/**
 Binds a parameter to the size of the output texture.
 Must be applied to a variable of type `int2`.
 Only available for `ComputeTexture`s.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_texture_TextureSizeAttribute {};

/**
 Binds a parameter to the internal size of the render buffer (size before upscaling) with which textures are created by default.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for `ComputeTexture`s.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_texture_OutputTextureAttribute {};

/**
 Binds a parameter to the scene data block uniform buffer.
 Must be applied to a ConstantBuffer of type `gd::compositor::SceneDataBlock`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SceneDataAttribute {};

/**
 Binds a parameter to the internal size of the render buffer (size before upscaling) with which textures are created by default.
 Must be applied to a variable of type `int2`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_InternalSizeAttribute {};

/**
 Binds a parameter to the color texture we are rendering 3D content to for the current view.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_ColorTextureAttribute {};

/**
 Binds a parameter to the depth texture we are rendering 3D content to for the current view.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_DepthTextureAttribute {};

/**
 Binds a parameter to the velocity texture we are rendering 3D content to for the current view.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_VelocityTextureAttribute {};

/**
 Binds a parameter to the texture with the provided context and name.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SceneBufferAttribute {
    let context : string = "render_buffers";
    let name : string;
};

/**
 Binds a parameter to a new texture.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_TextureAttribute {
    let format : compositor::DataFormat = compositor::DataFormat.DATA_FORMAT_R16G16B16A16_SNORM;
};

/**
 Binds a parameter to the texture with this name.
 Must be applied to a variable of type `Texture2D` or `Sampler2D`.
 Only available for compositor effects.
*/
[gd::Autobind]
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_TextureNameAttribute {
    let name : string;
};

/**
 Only run the associated kernel on the first dispatch of a compositor effect. May still be dispatched manually.
 Must be applied to a compute kernel function.
*/
[__AttributeUsage(_AttributeTargets.Function)]
public struct gd_compositor_OnceAttribute {};

/**
 Skips the associated kernel when running as part of a compositor effect. May still be dispatched manually.
 Must be applied to a compute kernel function.
*/
[__AttributeUsage(_AttributeTargets.Function)]
public struct gd_compositor_SkipAttribute {};
