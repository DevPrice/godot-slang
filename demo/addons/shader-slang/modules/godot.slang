module godot;

__include "scene_data";
__include "data_format";

namespace gd {

enum VariantType {
    NIL,

    // atomic types
    BOOL,
    INT,
    FLOAT,
    STRING,

    // math types
    VECTOR2,
    VECTOR2I,
    RECT2,
    RECT2I,
    VECTOR3,
    VECTOR3I,
    TRANSFORM2D,
    VECTOR4,
    VECTOR4I,
    PLANE,
    QUATERNION,
    AABB,
    BASIS,
    TRANSFORM3D,
    PROJECTION,

    // misc types
    COLOR,
    STRING_NAME,
    NODE_PATH,
    RID,
    OBJECT,
    CALLABLE,
    SIGNAL,
    DICTIONARY,
    ARRAY,

    // typed arrays
    PACKED_BYTE_ARRAY,
    PACKED_INT32_ARRAY,
    PACKED_INT64_ARRAY,
    PACKED_FLOAT32_ARRAY,
    PACKED_FLOAT64_ARRAY,
    PACKED_STRING_ARRAY,
    PACKED_VECTOR2_ARRAY,
    PACKED_VECTOR3_ARRAY,
    PACKED_COLOR_ARRAY,
    PACKED_VECTOR4_ARRAY,

    VARIANT_MAX
};

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

[__AttributeUsage(_AttributeTargets.Struct)]
struct gd_TypeAttribute {
    let type : VariantType;
};

[gd::Type(VariantType.RECT2I)]
public struct IntRect {
    public let position : int2;
    public let size : int2;

    public func contains(int2 point) -> bool {
        return point.x >= position.x && point.y >= position.y &&
               point.x < position.x + size.x && point.y < position.y + size.y;
    }
}

[gd::Type(VariantType.RECT2)]
public struct Rect {
	let position : float2;
	let size : float2;

    public func contains(float2 point) -> bool {
		return point.x >= position.x && point.y >= position.y &&
		       point.x < position.x + size.x && point.y < position.y + size.y;
	}
}

[gd::Type(VariantType.AABB)]
public struct AABB {
    public let position : float3;
    public let size : float3;

    public func get_end() -> float3 {
		return position + size;
    }
}

[gd::Type(VariantType.PLANE)]
public struct Plane {
    public let data : float4;

    public func get_normal() -> float3 {
        return data.xyz;
    }

    public func get_d() -> float {
        return data.w;
    }
}

[gd::Type(VariantType.QUATERNION)]
public struct Quaternion {
    public let data : float4;

    public func get_normal() -> float3 {
        return data.xyz;
    }

    public func get_d() -> float {
        return data.w;
    }
}

}

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
 Marks a variable or function as exported, making it visible in the Godot editor.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_ExportAttribute {};

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
    let repeat_mode = gd::SamplerRepeatMode.REPEAT;
};

/**
 Binds a parameter a nearest-neighbor sampler with the provided repeat mode.
 Must be applied to a variable of type `SamplerState` or `Sampler2D`.
*/
[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_NearestSamplerAttribute {
    let repeat_mode = gd::SamplerRepeatMode.REPEAT;
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
    let format : gd::compositor::DataFormat = gd::compositor::DataFormat.DATA_FORMAT_R16G16B16A16_SNORM;
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
