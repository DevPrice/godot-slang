#pragma once

#include <functional>
#include <optional>
#include <unordered_map>

#include "compute_shader_shape.h"

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
    DECLARE_GODOT_ATTRIBUTE(kernel_group, KernelGroup)
    DECLARE_GODOT_ATTRIBUTE(mouse_position, MousePosition)
    DECLARE_GODOT_ATTRIBUTE(name, Name)
    DECLARE_GODOT_ATTRIBUTE(property_hint, PropertyHint)
    DECLARE_GODOT_ATTRIBUTE(sampler, Sampler)
    DECLARE_GODOT_ATTRIBUTE(time, Time)
    DECLARE_GODOT_ATTRIBUTE(type, Type)
};

struct CompositorAttributes {
    DECLARE_COMPOSITOR_ATTRIBUTE(color_texture, ColorTexture)
    DECLARE_COMPOSITOR_ATTRIBUTE(depth_texture, DepthTexture)
    DECLARE_COMPOSITOR_ATTRIBUTE(once, Once)
    DECLARE_COMPOSITOR_ATTRIBUTE(internal_size, InternalSize)
    DECLARE_COMPOSITOR_ATTRIBUTE(context, Context)
    DECLARE_COMPOSITOR_ATTRIBUTE(scene_data, SceneData)
    DECLARE_COMPOSITOR_ATTRIBUTE(skip, Skip)
    DECLARE_COMPOSITOR_ATTRIBUTE(create_texture, CreateTexture)
    DECLARE_COMPOSITOR_ATTRIBUTE(texture_name, TextureName)
    DECLARE_COMPOSITOR_ATTRIBUTE(texture_size, TextureSize)
    DECLARE_COMPOSITOR_ATTRIBUTE(velocity_texture, VelocityTexture)
};

struct TextureAttributes {
    DECLARE_TEXTURE_ATTRIBUTE(output_size, OutputSize)
    DECLARE_TEXTURE_ATTRIBUTE(output_texture, OutputTexture)
};

template <>
struct std::hash<StringName>
{
    std::size_t operator()(const StringName& k) const noexcept {
        return k.hash();
    }
};

class AttributeRegistry {

public:
    using WriteHandler = std::function<void(Variant&)>;
    template <typename T>
    using AttributeHandlerFactory = std::function<T(const Dictionary&, const ShaderTypeLayoutShape&)>;

    template <typename T>
    struct FactoryWithPriority {
        AttributeHandlerFactory<T> factory{};
        int64_t priority{};
    };

private:
    std::unordered_map<StringName, FactoryWithPriority<WriteHandler>> write_handler_factories;

public:
    AttributeRegistry();
    AttributeRegistry(AttributeRegistry&) = delete;
    AttributeRegistry(AttributeRegistry&&) = delete;
    AttributeRegistry& operator=(const AttributeRegistry&) = delete;
    AttributeRegistry& operator=(AttributeRegistry&&) = delete;

    void register_write_handler(const StringName& attribute_name, const AttributeHandlerFactory<WriteHandler>& factory, int64_t priority = PRIORITY_DEFAULT);
    void register_write_handler(const StringName& attribute_name, const Callable& factory_callable, int64_t priority = PRIORITY_DEFAULT);
    std::optional<FactoryWithPriority<WriteHandler>> get_write_handler(const StringName& attribute_name);

    static AttributeRegistry* get_instance();

    static constexpr int64_t PRIORITY_DEFAULT = 0;
    static constexpr int64_t PRIORITY_MODIFIER = -10;
};
