#include "compute_shader_cursor.h"

#include "attributes.h"
#include "rdbuffer.h"
#include "variant_serializer.h"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_device.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/texture.hpp"
#include "godot_cpp/classes/uniform_set_cache_rd.hpp"

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
	result.binding_offset = static_cast<int64_t>(field.get("binding_offset", 0));
	result.byte_offset = static_cast<int64_t>(field.get("offset", 0));
	return result;
}

SamplerCache::SamplerCache(RenderingDevice* p_rendering_device) : rd(p_rendering_device) {
	cache.resize(RenderingDevice::SAMPLER_REPEAT_MODE_MAX * 2);
	ERR_FAIL_NULL(rd);
}

SamplerCache::~SamplerCache() {
	ERR_FAIL_NULL(rd);
	for (const RID rid : cache) {
		if (rid.is_valid()) {
			rd->free_rid(rid);
		}
	}
}

RID SamplerCache::get_sampler(const Ref<RDSamplerState>& sampler_state) {
	ERR_FAIL_NULL_V(sampler_state, {});
	// TODO: Support the rest of the properties
	const RenderingDevice::SamplerFilter filter = sampler_state->get_mag_filter();
	const RenderingDevice::SamplerRepeatMode repeat_mode = sampler_state->get_repeat_u();

	ERR_FAIL_INDEX_V(filter, 2, RID{});
	ERR_FAIL_INDEX_V(repeat_mode, RenderingDevice::SAMPLER_REPEAT_MODE_MAX, RID{});
	const int64_t sampler_index = filter * 2 + repeat_mode;
	if (RID cached_value = cache[sampler_index]; cached_value.is_valid()) {
		return cached_value;
	}

	RID sampler_rid = rd->sampler_create(sampler_state);
	cache[sampler_index] = sampler_rid;
	return sampler_rid;
}

ComputeShaderObject::ComputeShaderObject(RenderingDevice* p_rendering_device, SamplerCache* p_sampler_cache, const Ref<StructTypeLayoutShape>& p_shape) : rd(p_rendering_device), sampler_cache(p_sampler_cache), shape(p_shape) {
	ERR_FAIL_NULL(rd);
	ERR_FAIL_NULL(p_shape);
	for (const Dictionary binding : p_shape->get_bindings()) {
		if (binding.has("size")) {
			const int64_t size = binding["size"];
			const int64_t alignment = binding.get("alignment", 1);
			if (binding.has("uniform_type")) {
				const int64_t binding_index = binding.get("slot_offset", 0);
				const int64_t binding_space = binding.get("space_offset", 0);
				Ref<RDBuffer> buffer_data{};
				buffer_data.instantiate();
				buffer_data->set_alignment(alignment);
				buffer_data->set_size(size);
				buffer_data->set_is_fixed_size(true);
				buffers.set(Vector2i(binding_space, binding_index), buffer_data);
			} else {
				push_constants.resize(RDBuffer::aligned_size(size, alignment));
			}
		} else if (binding.has("uniform_type")) {
			const int64_t uniform_type = binding["uniform_type"];
			if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_STORAGE_BUFFER) {
				const int64_t binding_index = binding.get("slot_offset", 0);
				const int64_t binding_space = binding.get("space_offset", 0);
				Ref<RDBuffer> buffer_data{};
				buffer_data.instantiate();
				buffer_data->set_size(256); // TODO: Default sizing behavior?
				buffer_data->set_is_fixed_size(false);
				buffers.set(Vector2i(binding_space, binding_index), buffer_data);
			}
		}
	}
}

void ComputeShaderObject::write_resource(const ComputeShaderOffset& offset, const Variant& data) {
	ERR_FAIL_NULL(shape);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX(offset.binding_offset, bindings.size());
	const Dictionary binding = bindings[offset.binding_offset];
	ERR_FAIL_COND(!binding.has("uniform_type"));

	const auto uniform_type = static_cast<RenderingDevice::UniformType>(static_cast<int64_t>(binding["uniform_type"]));
	RDUniform& uniform = _get_uniform(binding["space_offset"], binding["slot_offset"]);
	uniform.set_uniform_type(uniform_type);
	uniform.clear_ids();

	const Variant data_or_default = data.get_type() == Variant::Type::NIL ? _get_default_value(uniform_type) : data;
	if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_UNIFORM_BUFFER || uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_STORAGE_BUFFER) {
		buffers.erase(Vector2i(binding["space_offset"], binding["slot_offset"]));
		uniform.add_id(_get_resource_rid(data_or_default));
	} else if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE && data_or_default.get_type() != Variant::Type::ARRAY) {
		if (const Object* sampler = data_or_default; sampler && sampler->is_class(RDSamplerState::get_class_static())) {
			const Variant default_texture = _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE);
			uniform.add_id(_get_resource_rid(sampler));
			uniform.add_id(_get_resource_rid(default_texture));
		} else {
			const Variant default_sampler = _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER);
			uniform.add_id(_get_resource_rid(default_sampler));
			uniform.add_id(_get_resource_rid(data_or_default));
		}
	} else if (data_or_default.get_type() == Variant::Type::ARRAY) {
		Array array = data_or_default;
		for (const Variant& element : array) {
			uniform.add_id(_get_resource_rid(element));
		}
	} else {
		uniform.add_id(_get_resource_rid(data_or_default));
	}
}

void ComputeShaderObject::write(const ComputeShaderOffset& offset, const Variant& data, const int64_t size, const ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) {
	ERR_FAIL_NULL(shape);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX(offset.binding_offset, bindings.size());
	const Dictionary binding = bindings[offset.binding_offset];
	if (binding.has("uniform_type")) {
		RDBuffer& buffer = _get_buffer(binding["space_offset"], binding["slot_offset"]);
		if (!buffer.get_is_fixed_size() && offset.byte_offset + size > buffer.get_buffer().size()) {
			buffer.set_size(offset.byte_offset + size);
		}
		buffer.write(offset.byte_offset, size, data, matrix_layout);
	} else {
		ERR_FAIL_COND(offset.byte_offset + size > push_constants.size());
		const VariantSerializer::Buffer buffer = VariantSerializer::serialize(data, matrix_layout);
		buffer.copy(push_constants.ptrw() + offset.byte_offset, size);
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

Variant ComputeShaderObject::_get_default_value(const RenderingDevice::UniformType type) {
	switch (type) {
		case RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER:
			return Ref(memnew(RDSamplerState));
		case RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE:
			return Array { _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER), _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE) };
		case RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE:
		case RenderingDevice::UniformType::UNIFORM_TYPE_IMAGE: {
			if (default_texture.is_null()) {
				default_texture.instantiate();
			}
			return default_texture;
		}
		default:
			return {};
	}
}

RID ComputeShaderObject::_get_resource_rid(const Variant& data) const {
	if (const Object* texture = data; texture && texture->is_class(Texture::get_class_static())) {
		return RenderingServer::get_singleton()->texture_get_rd_texture(data);
	}
	if (const auto sampler = Object::cast_to<RDSamplerState>(data)) {
		ERR_FAIL_NULL_V(sampler_cache, {});
		return sampler_cache->get_sampler(sampler);
	}
	return data;
}

ComputeShaderCursor ComputeShaderCursor::field(const StringName& path) const {
    const PackedStringArray parts = path.split("/");
    ComputeShaderCursor current(*this);
    for (const String& field_name : parts) {
    	const std::optional<Dictionary> property = shape->field(field_name);
    	ERR_FAIL_COND_V(!property, ComputeShaderCursor(nullptr));
        const Ref<ShaderTypeLayoutShape> property_shape = property->get("shape", nullptr);
        ERR_FAIL_NULL_V(property_shape, ComputeShaderCursor(nullptr));

        current.shape = property_shape;
		current.offset += ComputeShaderOffset::from_field(*property);

    	current.write_handlers.clear();
    	const Dictionary attributes = property->get("user_attributes", {});
    	for (auto attribute_name : attributes.keys()) {
    		const Dictionary attribute_arguments = attributes[attribute_name];
		    if (const auto factory = AttributeRegistry::get_instance()->get_write_handler(attribute_name)) {
    			if (const auto handler = factory->factory(attribute_arguments, *property_shape.ptr())) {
    				current.write_handlers.insert(WriteHandlerWithPriority{handler, factory->priority});
    			}
    		}
    	}
    	current.default_value = property->get("default_value", {});
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

void ComputeShaderCursor::write_bytes(const Variant& data, const int64_t size, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) const {
	ERR_FAIL_NULL(object);
	object->write(offset, data, size, matrix_layout);
}

void ComputeShaderCursor::write_resource(const Variant& data) const {
	ERR_FAIL_NULL(object);
	object->write_resource(offset, data);
}

void ComputeShaderCursor::write(const Variant& data) const {
	const Variant mutated_data = _apply_write_handlers(data);
	switch (mutated_data.get_type()) {
		case Variant::Type::PACKED_BYTE_ARRAY: {
			const PackedByteArray& bytes = mutated_data;
			const int64_t size = bytes.size();
			write_bytes(mutated_data, size);
			break;
		}
		case Variant::Type::RID:
			write_resource(mutated_data);
			break;
		default:
			ERR_FAIL_NULL(shape);
			shape->write_into(*this, mutated_data);
			break;
	}
}

Variant ComputeShaderCursor::_apply_write_handlers(Variant data) const {
	for (const auto [handler, _] : write_handlers) {
		handler(data, dispatch_context);
	}
	return data;
}
