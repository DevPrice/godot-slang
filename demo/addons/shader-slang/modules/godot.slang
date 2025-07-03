namespace gd {
    public enum SamplerRepeatMode {
        REPEAT = 0,
        MIRRORED_REPEAT = 1,
        CLAMP_TO_EDGE = 2,
        CLAMP_TO_BORDER = 3,
        MIRROR_CLAMP_TO_EDGE = 4,
    }
}

using gd;

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_LinearSamplerAttribute {
	SamplerRepeatMode repeat_mode = SamplerRepeatMode.REPEAT;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_NearestSamplerAttribute {
	SamplerRepeatMode repeat_mode = SamplerRepeatMode.REPEAT;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_ScreenTextureAttribute {};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_DepthTextureAttribute {};

[__AttributeUsage(_AttributeTargets.Var)]
public struct gd_compositor_SceneBufferAttribute {
	string context = "render_buffers";
	string name;
};
