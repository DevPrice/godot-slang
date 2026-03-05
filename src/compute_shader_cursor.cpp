#include "compute_shader_cursor.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_device.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/texture.hpp"

#include "attributes.h"
#include "rdbuffer.h"
#include "sampler_cache.h"
#include "variant_serializer.h"

#include "compute_shader_shape.h"

ComputeShaderOffset ComputeShaderOffset::operator+(const ComputeShaderOffset& other) const {
	return ComputeShaderOffset{
		binding_range_offset + other.binding_range_offset,
		element_offset + other.element_offset,
		byte_offset + other.byte_offset,
	};
}

ComputeShaderOffset& ComputeShaderOffset::operator+=(const ComputeShaderOffset& other) {
	binding_range_offset += other.binding_range_offset;
	element_offset += other.element_offset;
	byte_offset += other.byte_offset;
	return *this;
}

ComputeShaderOffset ComputeShaderOffset::from_field(const FieldShape& field) {
	ComputeShaderOffset result{};
	result.binding_range_offset = field.binding_offset;
	result.byte_offset = field.byte_offset;
	return result;
}

ComputeShaderObject::ComputeShaderObject(SamplerCache* p_sampler_cache, const Ref<ShaderTypeLayoutShape>& p_shape, const bool p_owns_binding_space, const int64_t p_first_slot_index) :
		sampler_cache(p_sampler_cache), shape(p_shape), owns_binding_space(p_owns_binding_space), first_slot_index(p_first_slot_index) {
	ERR_FAIL_NULL(p_shape);
	// TODO: Surely you can read directly if this should start a new space from the reflection API, but I can't find it
	bool has_only_parameter_blocks = true;
	int64_t binding_range_index = 0;
	for (const Dictionary binding : p_shape->get_bindings()) {
		has_only_parameter_blocks = has_only_parameter_blocks && static_cast<int64_t>(binding["binding_type"]) == static_cast<int64_t>(ShaderTypeLayoutShape::BindingType::PARAMETER_BLOCK);
		if (binding.has("size")) {
			const int64_t size = binding["size"];
			const int64_t alignment = binding.get("alignment", 1);
			if (binding.has("uniform_type")) {
				Ref<RDBuffer> buffer_data{};
				buffer_data.instantiate();
				buffer_data->set_alignment(alignment);
				buffer_data->set_size(size);
				buffer_data->set_is_fixed_size(true);
				buffers.set(binding_range_index, buffer_data);
			} else if (static_cast<int64_t>(binding["binding_type"]) == static_cast<int64_t>(ShaderTypeLayoutShape::BindingType::PUSH_CONSTANT)) {
				push_constants.resize(RDBuffer::aligned_size(size, alignment));
			}
		} else if (binding.has("uniform_type")) {
			const auto uniform_type = static_cast<RenderingDevice::UniformType>(static_cast<int64_t>(binding["uniform_type"]));
			if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_STORAGE_BUFFER) {
				Ref<RDBuffer> buffer_data{};
				buffer_data.instantiate();
				buffer_data->set_size(256); // TODO: Default sizing behavior?
				buffer_data->set_is_fixed_size(false);
				buffers.set(binding_range_index, buffer_data);
			}
		}
		if (binding.has("uniform_type") && !binding.has("leaf_shape")) {
			const auto uniform_type = static_cast<RenderingDevice::UniformType>(static_cast<int64_t>(binding["uniform_type"]));
			Ref<RDUniform> uniform{};
			uniform.instantiate();
			const int64_t slot_offset = binding["slot_offset"];
			uniform->set_binding(first_slot_index + slot_offset);
			uniform->set_uniform_type(uniform_type);
			uniforms.set(binding_range_index, uniform);
		}
		binding_range_index++;
	}
	if (has_only_parameter_blocks) {
		owns_binding_space = false;
	}
}

void ComputeShaderObject::write_resource(const ComputeShaderOffset& offset, const Variant& data) {
	ERR_FAIL_NULL(shape);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX(offset.binding_range_offset, bindings.size());
	const Dictionary binding = bindings[offset.binding_range_offset];
	ERR_FAIL_COND(!binding.has("uniform_type"));

	const auto uniform_type = static_cast<RenderingDevice::UniformType>(static_cast<int64_t>(binding["uniform_type"]));
	const Ref<RDUniform> uniform = uniforms.get(offset.binding_range_offset, {});
	ERR_FAIL_NULL(uniform);
	TypedArray<RID> rids = uniform->get_ids();
	uniform->clear_ids();

	const int64_t element_count = binding["binding_count"];
	ERR_FAIL_COND(element_count <= 0);
	const int64_t elements_per_binding = uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE ? 2 : 1;

	if (rids.size() != element_count * elements_per_binding) {
		rids.resize(element_count * elements_per_binding);
	}

	ERR_FAIL_INDEX(offset.element_offset, element_count);

	const Variant data_or_default = data == Variant{} ? _get_default_value(uniform_type) : data;
	if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_UNIFORM_BUFFER || uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_STORAGE_BUFFER) {
		buffers.erase(offset.binding_range_offset);
		rids[offset.element_offset] = _get_resource_rid(data_or_default);
	} else if (uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE && data_or_default.get_type() != Variant::Type::ARRAY) {
		if (const Object* sampler = data_or_default; sampler && sampler->is_class(RDSamplerState::get_class_static())) {
			const Variant default_texture = _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE);
			rids[offset.element_offset * 2] = _get_resource_rid(sampler);
			rids[offset.element_offset * 2 + 1] = _get_resource_rid(default_texture);
		} else {
			const Variant default_sampler = _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER);
			rids[offset.element_offset * 2] = _get_resource_rid(default_sampler);
			rids[offset.element_offset * 2 + 1] = _get_resource_rid(data_or_default);
		}
	} else if (data_or_default.get_type() == Variant::Type::ARRAY) {
		const Array array = data_or_default;
		for (int64_t i = 0; i < array.size() && offset.element_offset + i < rids.size(); i++) {
			rids[offset.element_offset + i] = _get_resource_rid(array[i]);
		}
	} else {
		rids[offset.element_offset] = _get_resource_rid(data_or_default);
	}

	for (const RID rid : rids) {
		uniform->add_id(rid);
	}
}

void ComputeShaderObject::write_bytes(const ComputeShaderOffset& offset, const Variant& data, const int64_t size, const ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) {
	ERR_FAIL_NULL(shape);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX(offset.binding_range_offset, bindings.size());
	const Dictionary binding = bindings[offset.binding_range_offset];
	if (binding.has("uniform_type")) {
		RDBuffer& buffer = _get_buffer(offset.binding_range_offset);
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
	for (const int64_t binding_range_index : buffers.keys()) {
		const Ref<RDBuffer> buffer = buffers[binding_range_index];
		if (buffer.is_valid()) {
			buffer->flush();
			const Ref<RDUniform> buffer_uniform = uniforms.get(binding_range_index, {});
			if (buffer_uniform.is_valid()) {
				buffer_uniform->clear_ids();
				buffer_uniform->add_id(buffer->get_rid());
			}
		}
	}
	for (auto it = subobjects.begin(); it != subobjects.end(); ++it) {
		it->second->flush_buffers();
	}
}

ComputeShaderObject::DescriptorSets ComputeShaderObject::get_descriptor_sets() {
	DescriptorSets result{};
	uint64_t next_space_index = 0;
	get_descriptor_sets(result, 0, next_space_index);
	return result;
}

void ComputeShaderObject::get_descriptor_sets(DescriptorSets& descriptor_sets, const uint64_t current_space_index, uint64_t& next_space_index) {
	const int64_t active_space_index = owns_binding_space ? next_space_index++ : current_space_index;
	descriptor_sets[active_space_index].append_array(uniforms.values());
	ERR_FAIL_NULL(shape);
	for (int64_t i = 0; i < shape->get_bindings().size(); i++) {
		if (ComputeShaderObject* subobject = get_or_create_subobject(i)) {
			subobject->get_descriptor_sets(descriptor_sets, active_space_index, next_space_index);
		}
	}
}

TypedArray<RID> ComputeShaderObject::get_rids(const ComputeShaderOffset& offset) const {
	const Ref<RDUniform> uniform = uniforms.get(offset.binding_range_offset, {});
	ERR_FAIL_NULL_V(uniform, {});
	ERR_FAIL_COND_V(uniform->get_ids().is_empty(), {});
	return uniform->get_ids();
}

PackedByteArray ComputeShaderObject::get_buffer_data(const ComputeShaderOffset& offset, const uint32_t size_bytes) const {
	const Ref<RDUniform> uniform = uniforms.get(offset.binding_range_offset, {});
	ERR_FAIL_NULL_V(uniform, {});
	ERR_FAIL_COND_V(uniform->get_ids().is_empty(), {});
	const RID buffer_rid = uniform->get_ids().front();
	return RenderingServer::get_singleton()->get_rendering_device()->buffer_get_data(buffer_rid, offset.byte_offset, size_bytes);
}

Error ComputeShaderObject::get_buffer_data_async(const Callable& callback, const ComputeShaderOffset& offset, const uint32_t size_bytes) const {
	const Ref<RDUniform> uniform = uniforms.get(offset.binding_range_offset, {});
	ERR_FAIL_NULL_V(uniform, {});
	ERR_FAIL_COND_V(uniform->get_ids().is_empty(), {});
	const RID buffer_rid = uniform->get_ids().front();
	return RenderingServer::get_singleton()->get_rendering_device()->buffer_get_data_async(buffer_rid, callback, offset.byte_offset, size_bytes);
}

ComputeShaderObject* ComputeShaderObject::get_or_create_subobject(const uint64_t binding_range_index) {
	if (const auto it = subobjects.find(binding_range_index); it != subobjects.end()) {
		return it->second.get();
	}
	ERR_FAIL_NULL_V(shape, nullptr);
	const TypedArray<Dictionary> bindings = shape->get_bindings();
	ERR_FAIL_INDEX_V(binding_range_index, bindings.size(), nullptr);
	const Dictionary binding = bindings[binding_range_index];
	const Ref<StructTypeLayoutShape> subshape = binding.get("leaf_shape", nullptr);
	if (subshape.is_null())
		return nullptr;
	const bool new_binding_space = static_cast<int64_t>(binding["binding_type"]) == static_cast<int64_t>(ShaderTypeLayoutShape::BindingType::PARAMETER_BLOCK);
	const int64_t subobject_first_slot = (new_binding_space ? 0 : first_slot_index) + static_cast<int64_t>(binding.get("slot_offset", 0));
	auto [it, _] = subobjects.emplace(binding_range_index, std::make_unique<ComputeShaderObject>(sampler_cache, subshape, new_binding_space, subobject_first_slot));
	return it->second.get();
}

RDBuffer& ComputeShaderObject::_get_buffer(const int64_t binding_range_index) {
	if (buffers.has(binding_range_index)) {
		const Ref<RDBuffer> buffer = buffers[binding_range_index];
		if (buffer.is_valid()) {
			return *buffer.ptr();
		}
	}
	Ref<RDBuffer> buffer{};
	buffer.instantiate();
	buffers.set(binding_range_index, buffer);
	return *buffer.ptr();
}

Variant ComputeShaderObject::_get_default_value(const RenderingDevice::UniformType type) {
	switch (type) {
		case RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER: {
			static Ref default_sampler_state = memnew(RDSamplerState);
			return default_sampler_state;
		}
		case RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE:
			return Array{ _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER), _get_default_value(RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE) };
		case RenderingDevice::UniformType::UNIFORM_TYPE_TEXTURE:
		case RenderingDevice::UniformType::UNIFORM_TYPE_IMAGE: {
			static Ref default_texture = memnew(PlaceholderTexture2D);
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
		ERR_FAIL_NULL_V(current.shape, ComputeShaderCursor(nullptr));
		const std::optional<FieldShape> property = current.shape->field(field_name);
		ERR_FAIL_COND_V(!property, ComputeShaderCursor(nullptr));
		const Ref<ShaderTypeLayoutShape> property_shape = property->shape;
		ERR_FAIL_NULL_V(property_shape, ComputeShaderCursor(nullptr));

		current.shape = property_shape;
		current.offset += ComputeShaderOffset::from_field(*property);

		current.write_handlers.clear();
		const Dictionary attributes = property->user_attributes;
		for (auto attribute_name : attributes.keys()) {
			const Dictionary attribute_arguments = attributes[attribute_name];
			if (const auto factory = AttributeRegistry::get_instance()->get_write_handler(attribute_name)) {
				if (const auto handler = factory->factory(attribute_arguments, *property)) {
					current.write_handlers.insert(WriteHandlerWithPriority{ handler, factory->priority });
				}
			}
		}
		current.default_value = property->default_value;

		if (ComputeShaderObject* subobject = current.object->get_or_create_subobject(current.offset.binding_range_offset)) {
			current.object = subobject;
			current.offset = {};
		}
	}
	return current;
}

ComputeShaderCursor ComputeShaderCursor::element(const int64_t index) const {
	const auto array_shape = Object::cast_to<ArrayTypeLayoutShape>(shape.ptr());
	ERR_FAIL_NULL_V(array_shape, ComputeShaderCursor(nullptr));

	ComputeShaderCursor result = *this;
	result.shape = array_shape->get_element_shape();
	result.offset.byte_offset += index * array_shape->get_stride();
	result.offset.element_offset *= array_shape->get_element_count();
	result.offset.element_offset += index;
	return result;
}

void ComputeShaderCursor::write_bytes(const Variant& data, const int64_t size, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) const {
	ERR_FAIL_NULL(object);
	object->write_bytes(offset, data, size, matrix_layout);
}

void ComputeShaderCursor::write_resource(const Variant& data) const {
	ERR_FAIL_NULL(object);
	object->write_resource(offset, data);
}

void ComputeShaderCursor::write(Variant data) const {
	for (const auto [handler, _] : write_handlers) {
		handler(data, dispatch_context);
	}
	switch (data.get_type()) {
		case Variant::Type::PACKED_BYTE_ARRAY: {
			const PackedByteArray& bytes = data;
			const int64_t size = bytes.size();
			write_bytes(data, size);
			break;
		}
		case Variant::Type::RID:
			write_resource(data);
			break;
		default:
			ERR_FAIL_NULL(shape);
			shape->write_into(*this, data);
			break;
	}
}

TypedArray<RID> ComputeShaderCursor::get_rids() const {
	ERR_FAIL_NULL_V(object, {});
	return object->get_rids(offset);
}

PackedByteArray ComputeShaderCursor::get_buffer_data() const {
	ERR_FAIL_NULL_V(object, {});
	ERR_FAIL_NULL_V(shape, {});
	return object->get_buffer_data(offset, shape->get_size());
}

Error ComputeShaderCursor::get_buffer_data_async(const Callable& callback) const {
	ERR_FAIL_NULL_V(object, {});
	ERR_FAIL_NULL_V(shape, {});
	return object->get_buffer_data_async(callback, offset, shape->get_size());
}
