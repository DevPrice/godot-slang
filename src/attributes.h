#pragma once

#include "godot_cpp/variant/variant.hpp"

using namespace godot;

#define DECLARE_ATTRIBUTE(function_name, attribute_name) static StringName& function_name() { \
    static StringName attribute(#attribute_name); \
    return attribute; \
}

#define DECLARE_GODOT_ATTRIBUTE(function_name, attribute_name) DECLARE_ATTRIBUTE(function_name, gd_##attribute_name)

#define DECLARE_COMPOSITOR_ATTRIBUTE(function_name, attribute_name) DECLARE_GODOT_ATTRIBUTE(function_name, compositor_##attribute_name)

#define DECLARE_TEXTURE_ATTRIBUTE(function_name, attribute_name) DECLARE_GODOT_ATTRIBUTE(function_name, texture_##attribute_name)

struct GodotAttributes {
    DECLARE_GODOT_ATTRIBUTE(autobind, Autobind)
    DECLARE_GODOT_ATTRIBUTE(class_name, Class)
    DECLARE_GODOT_ATTRIBUTE(color, Color)
    DECLARE_GODOT_ATTRIBUTE(default_white, DefaultWhite)
    DECLARE_GODOT_ATTRIBUTE(export_property, Export)
    DECLARE_GODOT_ATTRIBUTE(frame_id, FrameId)
    DECLARE_GODOT_ATTRIBUTE(global_param, GlobalParam)
    DECLARE_GODOT_ATTRIBUTE(linear_sampler, LinearSampler)
    DECLARE_GODOT_ATTRIBUTE(mouse_position, MousePosition)
    DECLARE_GODOT_ATTRIBUTE(name, Name)
    DECLARE_GODOT_ATTRIBUTE(nearest_sampler, NearestSampler)
    DECLARE_GODOT_ATTRIBUTE(time, Time)
    DECLARE_GODOT_ATTRIBUTE(type, Type)
};

struct CompositorAttributes {
    DECLARE_COMPOSITOR_ATTRIBUTE(color_texture, ColorTexture)
    DECLARE_COMPOSITOR_ATTRIBUTE(depth_texture, DepthTexture)
    DECLARE_COMPOSITOR_ATTRIBUTE(once, Once)
    DECLARE_COMPOSITOR_ATTRIBUTE(internal_size, InternalSize)
    DECLARE_COMPOSITOR_ATTRIBUTE(scene_buffer, SceneBuffer)
    DECLARE_COMPOSITOR_ATTRIBUTE(scene_data, SceneData)
    DECLARE_COMPOSITOR_ATTRIBUTE(skip, Skip)
    DECLARE_COMPOSITOR_ATTRIBUTE(texture, Texture)
    DECLARE_COMPOSITOR_ATTRIBUTE(texture_name, TextureName)
    DECLARE_COMPOSITOR_ATTRIBUTE(velocity_texture, VelocityTexture)
};

struct TextureAttributes {
    DECLARE_TEXTURE_ATTRIBUTE(output_texture, OutputTexture)
    DECLARE_TEXTURE_ATTRIBUTE(texture_size, TextureSize)
};
