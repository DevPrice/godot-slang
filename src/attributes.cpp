#include <memory>

#include "attributes.h"
#include "compute_dispatch_context.h"
#include "compute_texture.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/render_scene_buffers_rd.hpp"
#include "godot_cpp/classes/render_scene_data.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
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

RID get_black_texture() {
	static RID black_texture{};
	if (black_texture.is_valid()) {
		return black_texture;
	}
	PackedByteArray data{};
	constexpr int64_t data_size = 16 * 3;
	data.resize(data_size);
	memset(data.ptrw(), 0, data_size);
	const Ref black = memnew(Image);
	black->set_data(4, 4, false, Image::FORMAT_RGB8, data);
	black_texture = RenderingServer::get_singleton()->texture_2d_create(black);
	return black_texture;
}

AttributeRegistry::AttributeRegistry() {
	register_write_handler(GodotAttributes::color(), [](const Dictionary&, const Dictionary&) { return [](Variant& value, const Object*) {
																									handle_color_write(value);
																								}; }, PRIORITY_MODIFIER);
	register_write_handler(GodotAttributes::default_white(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object*) {
			if (value.get_type() == Variant::Type::NIL) {
				const auto rs = RenderingServer::get_singleton();
				value = rs->texture_get_rd_texture(rs->get_white_texture());
			}
		};
	});
	register_write_handler(GodotAttributes::default_black(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object*) {
			if (value.get_type() == Variant::Type::NIL) {
				const auto rs = RenderingServer::get_singleton();
				value = rs->texture_get_rd_texture(get_black_texture());
			}
		};
	});
	register_write_handler(GodotAttributes::frame_id(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object*) {
			if (value.get_type() == Variant::Type::NIL) {
				value = Engine::get_singleton()->get_frames_drawn();
			}
		};
	});
	register_write_handler(GodotAttributes::global_param(), [](const Dictionary& arguments, const Dictionary&) {
		const StringName param_name = arguments["name"];
		return [param_name](Variant& value, const Object*) {
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
	register_write_handler(GodotAttributes::mouse_position(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object*) {
			if (value.get_type() == Variant::Type::NIL) {
				if (const SceneTree* scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())) {
					if (const Window* window = scene_tree->get_root()) {
						value = Vector2i(window->get_mouse_position());
					}
				}
			}
		};
	});
	register_write_handler(GodotAttributes::sampler(), [](const Dictionary& arguments, const Dictionary& field) -> WriteHandler {
		const Ref<ShaderTypeLayoutShape> shape = field["shape"];
		const auto resource_shape = Object::cast_to<ResourceTypeLayoutShape>(shape.ptr());
		if (!resource_shape)
			return nullptr;

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

		return [sampler_state, uniform_type](Variant& value, const Object*) {
			if (value.get_type() == Variant::Type::NIL) {
				value = sampler_state;
			} else if (value.get_type() != Variant::Type::ARRAY && uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE) {
				value = Array{ sampler_state, value };
			}
		};
	});
	register_write_handler(GodotAttributes::time(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object*) {
			if (value.get_type() == Variant::Type::NIL) {
				value = Time::get_singleton()->get_ticks_msec() * .001f;
			}
		};
	});
	register_write_handler(CompositorAttributes::color_texture(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (const auto render_scene_buffers = effect_context->get_render_scene_buffers()) {
						value = render_scene_buffers->get_color_layer(effect_context->get_view());
					}
				}
			}
		};
	});
	static StringName key_name("name");
	static StringName key_context("context");
	static int64_t PRIORITY_CREATE_TEXTURE = PRIORITY_DEFAULT;
	register_write_handler(CompositorAttributes::create_texture(), [](const Dictionary& args, const Dictionary& field) {
		const int32_t texture_format = args.get("format", 0);
		const Dictionary attributes = field["user_attributes"];
		const Dictionary texture_name_attr = attributes.get(CompositorAttributes::texture_name(), {});
		const String texture_name = texture_name_attr.get(key_name, field["name"]);
		const Dictionary context_attr = attributes.get(CompositorAttributes::context(), {});
		const StringName context_name = context_attr.get(key_context, "__global_context");
		std::optional<Vector2i> texture_size_override = std::nullopt;
		if (attributes.has(CompositorAttributes::texture_size())) {
			static StringName key_size("size");
			const Dictionary size_args = attributes[CompositorAttributes::texture_size()];
			texture_size_override = size_args.get(key_size, {});
		}
		return [texture_format, texture_name, context_name, texture_size_override](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (RenderSceneBuffersRD* render_scene_buffers = effect_context->get_render_scene_buffers()) {
						const Vector2i texture_size = texture_size_override ? *texture_size_override : render_scene_buffers->get_internal_size();
						// TODO: Make more of this configurable
						value = render_scene_buffers->create_texture(
							context_name,
							texture_name,
							static_cast<RenderingDevice::DataFormat>(texture_format),
							RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_STORAGE_BIT,
							RenderingDevice::TEXTURE_SAMPLES_1,
							texture_size,
							1,
							1,
							false,
							false);
					}
				}
			}
		};
	}, PRIORITY_CREATE_TEXTURE);
	register_write_handler(CompositorAttributes::depth_texture(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (const auto render_scene_buffers = effect_context->get_render_scene_buffers()) {
						value = render_scene_buffers->get_depth_layer(effect_context->get_view());
					}
				}
			}
		};
	});
	register_write_handler(CompositorAttributes::internal_size(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (const auto render_scene_buffers = effect_context->get_render_scene_buffers()) {
						value = render_scene_buffers->get_internal_size();
					}
				}
			}
		};
	});
	register_write_handler(CompositorAttributes::scene_data(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (const RenderData* render_data = effect_context->get_render_data()) {
						if (const RenderSceneData* scene_data = render_data->get_render_scene_data()) {
							value = scene_data->get_uniform_buffer();
						}
					}
				}
			}
		};
	});
	register_write_handler(CompositorAttributes::texture_name(), [](const Dictionary& args, const Dictionary& field) {
		const String texture_name = args[key_name];
		const Dictionary attributes = field["user_attributes"];
		const Dictionary context_attr = attributes.get(CompositorAttributes::context(), {});
		const StringName context_name = context_attr.get(key_context, "__global_context");
		return [texture_name, context_name](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (const RenderSceneBuffersRD* render_scene_buffers = effect_context->get_render_scene_buffers()) {
						value = render_scene_buffers->get_texture(context_name, texture_name);
					}
				}
			}
		};
	}, PRIORITY_CREATE_TEXTURE - 1);
	register_write_handler(CompositorAttributes::velocity_texture(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto effect_context = Object::cast_to<CompositorEffectDispatchContext>(context)) {
					if (const auto render_scene_buffers = effect_context->get_render_scene_buffers()) {
						value = render_scene_buffers->get_velocity_layer(effect_context->get_view());
					}
				}
			}
		};
	});
	register_write_handler(TextureAttributes::output_size(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto texture_context = Object::cast_to<ComputeTextureDispatchContext>(context)) {
					value = texture_context->get_output_size();
				}
			}
		};
	});
	register_write_handler(TextureAttributes::output_texture(), [](const Dictionary&, const Dictionary&) {
		return [](Variant& value, const Object* context) {
			if (value.get_type() == Variant::Type::NIL && context) {
				if (const auto texture_context = Object::cast_to<ComputeTextureDispatchContext>(context)) {
					value = texture_context->get_output_texture_rid();
				}
			}
		};
	});
}

void AttributeRegistry::register_write_handler(const StringName& attribute_name, const AttributeHandlerFactory<WriteHandler>& factory, const int64_t priority) {
	write_handler_factories.insert_or_assign(attribute_name, FactoryWithPriority<WriteHandler>{ factory, priority });
}

void AttributeRegistry::register_write_handler(const StringName& attribute_name, const Callable& factory_callable, const int64_t priority) {
	const AttributeHandlerFactory<WriteHandler> factory = [factory_callable](const Dictionary& arguments, const Dictionary& field) -> WriteHandler {
		const Ref<ShaderTypeLayoutShape> shape = field["shape"];
		const Callable handler_callable = factory_callable.call(arguments, shape);
		if (handler_callable.is_valid()) {
			return [handler_callable](Variant& value, const Object* context) {
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
