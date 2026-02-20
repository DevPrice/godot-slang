#include <memory>

#include "attributes.h"

#include "compute_texture.h"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include "godot_cpp/classes/render_scene_data.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/classes/window.hpp"

void handle_color_write(Variant& value) {
    if (value.get_type() == Variant::Type::COLOR) {
        value = static_cast<Color>(value).srgb_to_linear();
    } else if (value.get_type() == Variant::Type::PACKED_COLOR_ARRAY) {
        PackedColorArray converted = value.duplicate();
        for (Color& color : converted) {
            color = color.srgb_to_linear();
        }
        value = converted;
    } else if (const Texture* texture = Object::cast_to<Texture>(value)) {
        value = RenderingServer::get_singleton()->texture_get_rd_texture(texture->get_rid(), true);
    } else if (value.get_type() == Variant::Type::ARRAY) {
        value = value.duplicate();
        for (Variant& element : static_cast<Array>(value)) {
            handle_color_write(element);
        }
    }
}

AttributeRegistry::AttributeRegistry() {
    register_write_handler(GodotAttributes::color(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext*) {
            handle_color_write(value);
        };
    }, PRIORITY_MODIFIER);
    register_write_handler(GodotAttributes::default_white(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext*) {
            if (value.get_type() == Variant::Type::NIL) {
                const auto rs = RenderingServer::get_singleton();
                value = rs->texture_get_rd_texture(rs->get_white_texture());
            }
        };
    });
    register_write_handler(GodotAttributes::frame_id(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext*) {
            if (value.get_type() == Variant::Type::NIL) {
                value = Engine::get_singleton()->get_frames_drawn();
            }
        };
    });
    register_write_handler(GodotAttributes::global_param(), [](const Dictionary& arguments, const ShaderTypeLayoutShape&) {
        const StringName param_name = arguments["name"];
        return [param_name](Variant& value, const ComputeDispatchContext*) {
            if (value.get_type() == Variant::Type::NIL) {
    #ifdef TOOLS_ENABLED
                if (unlikely(!RenderingServer::get_singleton()->is_on_render_thread())) {
                    static bool first_print = true;
                    if (first_print) {
                        UtilityFunctions::print("Avoid using the [gd::GlobalParam] attribute off of the render thread, it may cause performance issues.");
                        first_print = false;
                    }
                }
    #endif
                value = RenderingServer::get_singleton()->global_shader_parameter_get(param_name);
            }
        };
    });
    register_write_handler(GodotAttributes::mouse_position(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext*) {
            if (value.get_type() == Variant::Type::NIL) {
                if (const SceneTree* scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())) {
                    if (const Window* window = scene_tree->get_root()) {
                        value = Vector2i(window->get_mouse_position());
                    }
                }
            }
        };
    });
    register_write_handler(GodotAttributes::sampler(), [](const Dictionary& arguments, const ShaderTypeLayoutShape& shape) -> WriteHandler {
        const auto resource_shape = Object::cast_to<ResourceTypeLayoutShape>(&shape);
        if (!resource_shape) return nullptr;

        const RenderingDevice::UniformType uniform_type = resource_shape->get_uniform_type();

        int64_t filter_mode_int = arguments.get("filter", RenderingDevice::SAMPLER_FILTER_LINEAR);
        int64_t repeat_mode_int = arguments.get("repeat_mode", RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
        Ref<RDSamplerState> sampler_state;
        sampler_state.instantiate();
        sampler_state->set_min_filter(static_cast<RenderingDevice::SamplerFilter>(filter_mode_int));
        sampler_state->set_mag_filter(static_cast<RenderingDevice::SamplerFilter>(filter_mode_int));
        sampler_state->set_mip_filter(static_cast<RenderingDevice::SamplerFilter>(filter_mode_int));
        sampler_state->set_repeat_u(static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
        sampler_state->set_repeat_v(static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
        sampler_state->set_repeat_w(static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));

        return [sampler_state, uniform_type](Variant& value, const ComputeDispatchContext*) {
            if (value.get_type() == Variant::Type::NIL) {
                value = sampler_state;
            } else if (value.get_type() != Variant::Type::ARRAY && uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE) {
                value = Array { sampler_state, value };
            }
        };
    });
    register_write_handler(GodotAttributes::time(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext*) {
            if (value.get_type() == Variant::Type::NIL) {
                value = Time::get_singleton()->get_ticks_msec() * .001f;
            }
        };
    });
    register_write_handler(CompositorAttributes::color_texture(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext* context) {
            if (value.get_type() == Variant::Type::NIL && context) {
                if (const auto render_scene_buffers = context->get_render_scene_buffers()) {
                    value = render_scene_buffers->get_color_layer(context->get_view());
                }
            }
        };
    });
    register_write_handler(CompositorAttributes::depth_texture(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext* context) {
            if (value.get_type() == Variant::Type::NIL && context) {
                if (const auto render_scene_buffers = context->get_render_scene_buffers()) {
                    value = render_scene_buffers->get_depth_layer(context->get_view());
                }
            }
        };
    });
    register_write_handler(CompositorAttributes::internal_size(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext* context) {
            if (value.get_type() == Variant::Type::NIL && context) {
                if (const auto render_scene_buffers = context->get_render_scene_buffers()) {
                    value = render_scene_buffers->get_internal_size();
                }
            }
        };
    });
    register_write_handler(CompositorAttributes::scene_data(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext* context) {
            if (value.get_type() == Variant::Type::NIL && context) {
                if (const RenderData* render_data = context->get_render_data()) {
                    if (const RenderSceneData* scene_data = render_data->get_render_scene_data()) {
                        value = scene_data->get_uniform_buffer();
                    }
                }
            }
        };
    });
    register_write_handler(CompositorAttributes::velocity_texture(), [](const Dictionary&, const ShaderTypeLayoutShape&) {
        return [](Variant& value, const ComputeDispatchContext* context) {
            if (value.get_type() == Variant::Type::NIL && context) {
                if (const auto render_scene_buffers = context->get_render_scene_buffers()) {
                    value = render_scene_buffers->get_velocity_layer(context->get_view());
                }
            }
        };
    });
}

void AttributeRegistry::register_write_handler(const StringName& attribute_name, const AttributeHandlerFactory<WriteHandler>& factory, const int64_t priority) {
    write_handler_factories.insert_or_assign(attribute_name, FactoryWithPriority<WriteHandler>{factory, priority});
}

void AttributeRegistry::register_write_handler(const StringName& attribute_name, const Callable& factory_callable, const int64_t priority) {
    const AttributeHandlerFactory<WriteHandler> factory = [factory_callable](const Dictionary& arguments, const ShaderTypeLayoutShape& shape) -> WriteHandler {
        const Callable handler_callable = factory_callable.call(arguments, &shape);
        if (handler_callable.is_valid()) {
            return [handler_callable](Variant& value, const ComputeDispatchContext* context) {
                value = handler_callable.call(value, context);
            };
        }
        return nullptr;
    };
    register_write_handler(attribute_name, factory, priority);
}

std::optional<AttributeRegistry::FactoryWithPriority<AttributeRegistry::WriteHandler>> AttributeRegistry::get_write_handler(const StringName& attribute_name) {
    const auto it = write_handler_factories.find(attribute_name);
    return it != write_handler_factories.end() ? std::make_optional(it->second) : std::nullopt;
}

AttributeRegistry* AttributeRegistry::get_instance() {
    static auto instance = std::make_unique<AttributeRegistry>();
    return instance.get();
}
