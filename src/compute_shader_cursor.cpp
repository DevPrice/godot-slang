#include "compute_shader_cursor.h"

#include "attributes.h"
#include "rdbuffer.h"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/scene_tree.hpp"
#include "godot_cpp/classes/texture.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/classes/uniform_set_cache_rd.hpp"
#include "godot_cpp/classes/window.hpp"

ComputeShaderOffset ComputeShaderOffset::operator+(const ComputeShaderOffset& other) const {
	return ComputeShaderOffset{
		binding_offset + other.binding_offset,
		binding_index_offset + other.binding_index_offset,
		byte_offset + other.byte_offset,
	};
}

ComputeShaderOffset& ComputeShaderOffset::operator+=(const ComputeShaderOffset& other) {
	binding_offset += other.binding_offset;
	binding_index_offset += other.binding_index_offset;
	byte_offset += other.byte_offset;
	return *this;
}

ComputeShaderOffset ComputeShaderOffset::from_field(const Dictionary& field) {
	ComputeShaderOffset result{};
	result.binding_offset += static_cast<int64_t>(field.get("binding_offset", 0));
	result.byte_offset += static_cast<int64_t>(field.get("offset", 0));
	return result;
}

ComputeShaderObject::ComputeShaderObject(RenderingDevice* p_rendering_device, const Ref<StructTypeLayoutShape>& p_shape) : rd(p_rendering_device), shape(p_shape) {
	ERR_FAIL_NULL(rd);
	sampler_cache.resize(RenderingDevice::SAMPLER_REPEAT_MODE_MAX * 2);
	for (const Dictionary binding : p_shape->get_bindings()) {
		if (binding.has("size")) {
			const int64_t size = binding["size"];
			if (binding.has("uniform_type")) {
				const int64_t binding_index = binding.get("slot_offset", 0);
				const int64_t binding_space = binding.get("space_offset", 0);
				Ref<RDBuffer> buffer_data{};
				buffer_data.instantiate();
				buffer_data->set_size(size);
				buffer_data->set_is_fixed_size(size > 0);
				buffers.set(Vector2i(binding_space, binding_index), buffer_data);
			} else {
				push_constants.resize(size);
			}
		}
	}
}

ComputeShaderObject::~ComputeShaderObject() {
	ERR_FAIL_NULL(rd);
	for (const RID rid: sampler_cache) {
		if (rid.is_valid()) {
			rd->free_rid(rid);
		}
	}
}

void ComputeShaderObject::write_resource(const ComputeShaderOffset offset, const Variant& data) {
	ERR_FAIL_NULL(shape);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX(offset.binding_offset, bindings.size());
	const Dictionary binding = bindings[offset.binding_offset];

	RDUniform& uniform = _get_uniform(binding["space_offset"], binding["slot_offset"]);
	const auto uniform_type = static_cast<RenderingDevice::UniformType>(static_cast<int64_t>(binding["uniform_type"]));
	uniform.set_uniform_type(uniform_type);
	uniform.clear_ids();

	const Variant data_or_default = data.get_type() == Variant::Type::NIL ? _get_default_value(uniform_type) : data;
	if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_UNIFORM_BUFFER || uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_STORAGE_BUFFER) {
		buffers.erase(Vector2i(binding["space_offset"], binding["slot_offset"]));
		uniform.add_id(_get_resource_rid(data_or_default));
	} else if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE && data_or_default.get_type() != Variant::ARRAY) {
		const Variant sampler = _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER);
		uniform.add_id(_get_resource_rid(sampler));
		uniform.add_id(_get_resource_rid(data_or_default));
	} else if (data_or_default.get_type() == Variant::ARRAY) {
		Array array = data_or_default;
		for (const Variant& element : array) {
			uniform.add_id(_get_resource_rid(element));
		}
	} else {
		uniform.add_id(_get_resource_rid(data_or_default));
	}
}

void ComputeShaderObject::write(const ComputeShaderOffset offset, const Variant& data, const int64_t size, const ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) {
	ERR_FAIL_NULL(shape);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX(offset.binding_offset, bindings.size());
	const Dictionary binding = bindings[offset.binding_offset];
	if (binding.has("uniform_type")) {
		RDBuffer& buffer = _get_buffer(binding["space_offset"], binding["slot_offset"]);
		if (offset.byte_offset + size > buffer.get_buffer().size()) {
			buffer.set_size(offset.byte_offset + size);
		}
		buffer.write(offset.byte_offset, size, data, matrix_layout);
	} else {
		ERR_FAIL_COND(offset.byte_offset + size > push_constants.size());
		RDBuffer::write(push_constants, offset.byte_offset, size, data, matrix_layout);
	}
}

void ComputeShaderObject::flush_buffers() {
	for (const Vector2i key : buffers.keys()) {
		const Ref<RDBuffer> buffer = buffers[key];
		if (buffer.is_valid()) {
			buffer->flush();
			RDUniform& buffer_uniform = _get_uniform(key.x, key.y);
			buffer_uniform.set_uniform_type(buffer->get_uniform_type());
			buffer_uniform.clear_ids();
			buffer_uniform.add_id(buffer->get_rid());
		}
	}
}

void ComputeShaderObject::bind_uniforms(const int64_t compute_list, const RID& shader_rid) const {
	ERR_FAIL_NULL(rd);
	for (int64_t i = 0; i < uniforms.size(); i++) {
		Array binding_set = uniforms[i];
		if (!binding_set.is_empty()) {
			const RID uniform_set = UniformSetCacheRD::get_cache(shader_rid, i, binding_set);
			rd->compute_list_bind_uniform_set(compute_list, uniform_set, i);
		}
    }
    if (push_constants.size() > 0) {
        rd->compute_list_set_push_constant(compute_list, push_constants, push_constants.size());
    }
}

RDBuffer& ComputeShaderObject::_get_buffer(const int64_t binding_space, const int64_t binding_index) {
	const auto key = Vector2i(binding_space, binding_index);
	if (buffers.has(key)) {
		const Ref<RDBuffer> buffer = buffers[key];
		if (buffer.is_valid()) {
			return *buffer.ptr();
		}
	}
	Ref<RDBuffer> buffer{};
	buffer.instantiate();
	buffers.set(key, buffer);
	return *buffer.ptr();
}

RDUniform& ComputeShaderObject::_get_uniform(const int64_t binding_space, const int64_t binding_index) {
	if (uniforms.size() <= binding_space) {
		uniforms.resize(binding_space + 1);
	}
	Array uniform_set = uniforms[binding_space];
	if (uniform_set.size() <= binding_index) {
		uniform_set.resize(binding_index + 1);
	}
	Ref<RDUniform> uniform = uniform_set[binding_index];
	if (uniform.is_null()) {
		uniform.instantiate();
		uniform->set_binding(binding_index);
		uniform_set[binding_index] = uniform;
	}
	return *uniform.ptr();
}

RID ComputeShaderObject::_get_sampler(const RenderingDevice::SamplerFilter filter, const RenderingDevice::SamplerRepeatMode repeat_mode) {
	ERR_FAIL_INDEX_V(filter, 2, RID{});
	ERR_FAIL_INDEX_V(repeat_mode, RenderingDevice::SAMPLER_REPEAT_MODE_MAX, RID{});
	const int64_t sampler_index = filter * 2 + repeat_mode;
	if (RID cached_value = sampler_cache[sampler_index]; cached_value.is_valid()) {
		return cached_value;
	}

	const Ref sampler_state = memnew(RDSamplerState);
	sampler_state->set_min_filter(filter);
	sampler_state->set_mag_filter(filter);
	sampler_state->set_mip_filter(filter);
	sampler_state->set_repeat_u(repeat_mode);
	sampler_state->set_repeat_v(repeat_mode);
	sampler_state->set_repeat_w(repeat_mode);
	const RID sampler_rid = rd->sampler_create(sampler_state);
	sampler_cache[sampler_index] = sampler_rid;
	return sampler_rid;
}

Variant ComputeShaderObject::_get_default_value(const RenderingDevice::UniformType type) {
	switch (type) {
		case RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER:
			return Ref(memnew(RDSamplerState));
		case RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE:
			return Array { _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER), _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE) };
		case RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE:
		case RenderingDevice::UniformType::UNIFORM_TYPE_IMAGE:
			return RenderingServer::get_singleton()->get_test_texture();
		default:
			return {};
	}
}

RID ComputeShaderObject::_get_resource_rid(const Variant& data) {
	if (const Object* texture = data; texture && texture->is_class(Texture::get_class_static())) {
		return RenderingServer::get_singleton()->texture_get_rd_texture(data);
	}
	if (const auto sampler = Object::cast_to<RDSamplerState>(data)) {
		// TODO: Support the rest of the properties
		return _get_sampler(sampler->get_mag_filter(), sampler->get_repeat_u());
	}
	return data;
}

ComputeShaderCursor ComputeShaderCursor::field(const StringName& path) const {
    const PackedStringArray parts = path.split("/");
    ComputeShaderCursor current(*this);
    for (const String& field_name : parts) {
        const auto struct_shape = Object::cast_to<StructTypeLayoutShape>(current.shape.ptr());
        ERR_FAIL_NULL_V(struct_shape, ComputeShaderCursor(nullptr));

        const Dictionary properties = struct_shape->get_properties();
        ERR_FAIL_COND_V(!properties.has(field_name), ComputeShaderCursor(nullptr));

        const Dictionary property = properties[field_name];
        const Ref<ShaderTypeLayoutShape> property_shape = property.get("shape", nullptr);
        current.shape = property_shape;
		current.offset += ComputeShaderOffset::from_field(property);
    }
    return current;
}

ComputeShaderCursor ComputeShaderCursor::element(const int64_t index) const {
    const auto array_shape = Object::cast_to<ArrayTypeLayoutShape>(shape.ptr());
	ERR_FAIL_NULL_V(array_shape, ComputeShaderCursor(nullptr));

	ComputeShaderCursor result = *this;
	result.shape = array_shape->get_element_shape();
	result.offset.byte_offset += index * array_shape->get_stride();
    return result;
}

void ComputeShaderCursor::write(const Variant& data) const {
    ERR_FAIL_NULL(object);
    ERR_FAIL_NULL(shape);
	const auto resource_shape = Object::cast_to<ResourceTypeLayoutShape>(shape.ptr());
	if (data.get_type() == Variant::PACKED_BYTE_ARRAY || (resource_shape && resource_shape->get_resource_type() == ResourceTypeLayoutShape::RAW_BYTES)) {
		// TODO: Handle other types
		const PackedByteArray bytes = data;
		const int64_t size = bytes.size();
		object->write(offset, size, data);
	} else if (static_cast<RID>(data).is_valid() || resource_shape) {
		// TODO: If we're writing a texture to a sampler resource, we need to bind the sampler declared via the gd::Sampler attribute, if present
		// Right now, we'll always use the default sampler in that case
		object->write_resource(offset, data);
	} else if (const auto variant_shape = Object::cast_to<VariantTypeLayoutShape>(shape.ptr())) {
		const int64_t size = variant_shape->get_size();
		ERR_FAIL_COND(size <= 0);
		object->write(offset, data, size, variant_shape->get_matrix_layout());
	} else if (const auto structured_shape = Object::cast_to<StructTypeLayoutShape>(shape.ptr())) {
		const Dictionary properties = structured_shape->get_properties();

		for (const StringName property_name : properties.keys()) {
			const Dictionary property = properties[property_name];
			const Dictionary property_attributes = property["user_attributes"];

			bool is_valid{};
			Variant property_value = data.get_named(property_name, is_valid);
			if (property_value.get_type() == Variant::Type::NIL) {
				property_value = _get_default_value(property);
			}

			// TODO: If this gets any more complex, it needs to move out of here
			if (property_attributes.has(GodotAttributes::color())) {
				if (property_value.get_type() == Variant::COLOR) {
					property_value = static_cast<Color>(property_value).srgb_to_linear();
				}
				if (property_value.get_type() == Variant::PACKED_COLOR_ARRAY) {
					PackedColorArray converted = property_value.duplicate();
					for (Color& color : converted) {
						color = color.srgb_to_linear();
					}
					property_value = converted;
				}
			}

			field(property_name).write(property_value);
		}
	} else if (const auto array_shape = Object::cast_to<ArrayTypeLayoutShape>(shape.ptr())) {
		const int64_t stride = array_shape->get_stride();
		ERR_FAIL_COND(stride <= 0);

		Variant key;
		bool is_valid;
		if (data.iter_init(key, is_valid) && is_valid) {
			int64_t i = 0;
			do {
				Variant value = data.iter_get(key, is_valid);
				if (is_valid) {
					element(i++).write(value);
				}
			} while (data.iter_next(key, is_valid) && is_valid);
		}
	} else {
		UtilityFunctions::push_warning("Unable to write invalid shape: ", shape);
	}
}

Variant ComputeShaderCursor::_get_default_value(Dictionary property) {
	const Dictionary attributes = property["user_attributes"];
	if (attributes.has(GodotAttributes::global_param())) {
#ifdef TOOLS_ENABLED
		if (!RenderingServer::get_singleton()->is_on_render_thread()) {
			WARN_PRINT_ONCE("Avoid using the [gd::GlobalParam] attribute off of the render thread, it may cause performance issues.");
		}
#endif
		const Dictionary attribute = attributes[GodotAttributes::global_param()];
		const String param_name = attribute["name"];
		return RenderingServer::get_singleton()->global_shader_parameter_get(param_name);
	}

	// ReSharper disable once CppTooWideScope
	const int64_t layout_unit = property.get("layout_unit", 0);
	switch (layout_unit) {
		case ShaderTypeLayoutShape::LayoutUnit::UNIFORM:
		case ShaderTypeLayoutShape::LayoutUnit::PUSH_CONSTANT_BUFFER:
			if (attributes.has(GodotAttributes::time())) {
				return Time::get_singleton()->get_ticks_msec() * .001f;
			}
			if (attributes.has(GodotAttributes::frame_id())) {
				return Engine::get_singleton()->get_frames_drawn();
			}
			if (attributes.has(GodotAttributes::mouse_position())) {
				if (const SceneTree* scene_tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop())) {
					if (const Window* window = scene_tree->get_root()) {
						return Vector2i(window->get_mouse_position());
					}
				}
			}
			break;
		default:
			if (attributes.has(GodotAttributes::sampler())) {
				const Dictionary sampler_attribute = attributes[GodotAttributes::sampler()];
				int64_t filter_mode_int = sampler_attribute.get("filter", RenderingDevice::SAMPLER_FILTER_LINEAR);
				int64_t repeat_mode_int = sampler_attribute.get("repeat_mode", RenderingDevice::SAMPLER_REPEAT_MODE_REPEAT);
				Ref<RDSamplerState> sampler_state;
				sampler_state.instantiate();
				sampler_state->set_min_filter(static_cast<RenderingDevice::SamplerFilter>(filter_mode_int));
				sampler_state->set_mag_filter(static_cast<RenderingDevice::SamplerFilter>(filter_mode_int));
				sampler_state->set_mip_filter(static_cast<RenderingDevice::SamplerFilter>(filter_mode_int));
				sampler_state->set_repeat_u(static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
				sampler_state->set_repeat_v(static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
				sampler_state->set_repeat_w(static_cast<RenderingDevice::SamplerRepeatMode>(repeat_mode_int));
				return sampler_state;
			}
			if (attributes.has(GodotAttributes::default_white())) {
				return RenderingServer::get_singleton()->get_white_texture();
			}
			break;
	}

	if (const ResourceTypeLayoutShape* resource_shape = Object::cast_to<ResourceTypeLayoutShape>(property.get("shape", nullptr))) {
		switch (resource_shape->get_uniform_type()) {
			case RenderingDevice::UNIFORM_TYPE_SAMPLER:
				// TODO
				break;
			case RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE:
			case RenderingDevice::UNIFORM_TYPE_TEXTURE: {
				const RID default_texture = attributes.has(GodotAttributes::default_white())
					? RenderingServer::get_singleton()->get_white_texture()
					: RenderingServer::get_singleton()->get_test_texture();
				return RenderingServer::get_singleton()->texture_get_rd_texture(default_texture);
			}
			default:
				break;
		}
	}

	return property.get("default_value", Variant{});
}
