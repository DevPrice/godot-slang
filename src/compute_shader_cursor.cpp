#include "compute_shader_cursor.h"

#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_device.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/classes/texture.hpp"

#include "attributes.h"
#include "compute_buffer.h"
#include "sampler_cache.h"
#include "variant_serializer.h"

#include "compute_shader_shape.h"

using namespace godot;

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

ComputeShaderObject::ComputeShaderObject(RenderingDevice* p_rendering_device, SamplerCache* p_sampler_cache, const Ref<ShaderTypeLayoutShape>& p_shape, const bool p_owns_binding_space, const int64_t p_first_slot_index) :
		rendering_device(p_rendering_device), sampler_cache(p_sampler_cache), shape(p_shape), owns_binding_space(p_owns_binding_space), first_slot_index(p_first_slot_index) {
	ERR_FAIL_NULL(p_shape);
	// TODO: Surely you can read directly if this should start a new space from the reflection API, but I can't find it
	bool has_only_parameter_blocks = true;
	const TypedArray<Dictionary> bindings = p_shape->get_bindings();
	for (int64_t binding_range_index = 0; binding_range_index < bindings.size(); binding_range_index++) {
		const auto binding = BindingRange::from_dict(bindings[binding_range_index]);
		has_only_parameter_blocks = has_only_parameter_blocks && binding.type == ShaderTypeLayoutShape::BindingType::PARAMETER_BLOCK;
		if (binding.uniform_type && binding.leaf_shape.is_null()) {
			Ref<RDUniform> uniform{};
			uniform.instantiate();
			uniform->set_binding(first_slot_index + binding.slot_offset);
			uniform->set_uniform_type(*binding.uniform_type);
			uniforms.set(binding_range_index, uniform);
		}
		if (binding.type == ShaderTypeLayoutShape::BindingType::PUSH_CONSTANT) {
			push_constants.resize(ComputeBuffer::aligned_size(binding.size, binding.alignment));
		}
	}
	if (has_only_parameter_blocks) {
		owns_binding_space = false;
	}
}

void ComputeShaderObject::write_resource(const ComputeShaderOffset& offset, const Variant& data) {
	if (const RDUniform* uniform = Object::cast_to<RDUniform>(data)) {
		uniforms.set(offset.binding_range_offset, uniform);
		return;
	}

	const auto binding = _get_binding_range(offset.binding_range_offset);
	ERR_FAIL_COND(!binding);
	ERR_FAIL_COND(!binding->uniform_type);
	ERR_FAIL_COND(binding->binding_count <= 0);

	const auto uniform_type = *binding->uniform_type;
	const Ref<RDUniform> uniform = uniforms.get(offset.binding_range_offset, {});
	ERR_FAIL_NULL(uniform);
	TypedArray<RID> rids = uniform->get_ids();
	uniform->clear_ids();

	const int64_t elements_per_binding = uniform_type == RenderingDevice::UniformType::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE ? 2 : 1;

	if (rids.size() != binding->binding_count * elements_per_binding) {
		rids.resize(binding->binding_count * elements_per_binding);
	}

	ERR_FAIL_INDEX(offset.element_offset, binding->binding_count);

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
	const auto binding_range = _get_binding_range(offset.binding_range_offset);
	ERR_FAIL_COND(!binding_range);
	if (binding_range->type == ShaderTypeLayoutShape::BindingType::PUSH_CONSTANT) {
		ERR_FAIL_COND(offset.byte_offset + size > push_constants.size());
		const VariantSerializer::Buffer buffer = VariantSerializer::serialize(data, BufferLayout::STD430, matrix_layout);
		buffer.copy(push_constants.ptrw() + offset.byte_offset, size);
	} else {
		ComputeBuffer* buffer = _get_or_create_buffer(offset.binding_range_offset);
		ERR_FAIL_NULL(buffer);
		if (!buffer->get_is_fixed_size() && offset.byte_offset + size > buffer->get_buffer().size()) {
			buffer->set_size(offset.byte_offset + size);
		}
		buffer->write(offset.byte_offset, size, data, matrix_layout);
	}
}

void ComputeShaderObject::flush_buffers() {
	for (auto& [binding_range_index, buffer] : buffers) {
		buffer->flush();
		const Ref<RDUniform> buffer_uniform = uniforms.get(binding_range_index, {});
		if (buffer_uniform.is_valid()) {
			buffer_uniform->clear_ids();
			buffer_uniform->add_id(buffer->get_rid());
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
	ERR_FAIL_NULL_V(rendering_device, {});
	return rendering_device->buffer_get_data(buffer_rid, offset.byte_offset, size_bytes);
}

Error ComputeShaderObject::get_buffer_data_async(const Callable& callback, const ComputeShaderOffset& offset, const uint32_t size_bytes) const {
	const Ref<RDUniform> uniform = uniforms.get(offset.binding_range_offset, {});
	ERR_FAIL_NULL_V(uniform, {});
	ERR_FAIL_COND_V(uniform->get_ids().is_empty(), {});
	const RID buffer_rid = uniform->get_ids().front();
	ERR_FAIL_NULL_V(rendering_device, {});
	return rendering_device->buffer_get_data_async(buffer_rid, callback, offset.byte_offset, size_bytes);
}

ComputeShaderObject* ComputeShaderObject::get_or_create_subobject(const uint64_t binding_range_index) {
	if (const auto it = subobjects.find(binding_range_index); it != subobjects.end()) {
		return it->second.get();
	}
	const auto binding_range = _get_binding_range(binding_range_index);
	ERR_FAIL_COND_V(!binding_range, nullptr);
	if (binding_range->leaf_shape.is_null())
		return nullptr;
	const bool new_binding_space = binding_range->type == ShaderTypeLayoutShape::BindingType::PARAMETER_BLOCK;
	const int64_t subobject_first_slot = (new_binding_space ? 0 : first_slot_index) + binding_range->slot_offset;
	auto [it, _] = subobjects.emplace(binding_range_index, std::make_unique<ComputeShaderObject>(rendering_device, sampler_cache, binding_range->leaf_shape, new_binding_space, subobject_first_slot));
	return it->second.get();
}

std::optional<BindingRange> ComputeShaderObject::_get_binding_range(const int64_t binding_range_index) const {
	ERR_FAIL_NULL_V(shape, std::nullopt);
	const TypedArray<Dictionary> binding_ranges = shape->get_bindings();
	ERR_FAIL_INDEX_V(binding_range_index, binding_ranges.size(), std::nullopt);
	const Dictionary binding = binding_ranges[binding_range_index];
	return BindingRange::from_dict(binding);
}

ComputeBuffer* ComputeShaderObject::_get_or_create_buffer(const int64_t binding_range_index) {
	const auto it = buffers.find(binding_range_index);
	if (it != buffers.end()) {
		return it->second.get();
	}
	ERR_FAIL_NULL_V(shape, nullptr);
	if (const auto binding_range = _get_binding_range(binding_range_index)) {
		if (binding_range->leaf_shape.is_valid()) return nullptr;
		switch (binding_range->base_binding_type()) {
			case ShaderTypeLayoutShape::BindingType::CONSTANT_BUFFER: {
				auto [new_buffer_it, _] = buffers.try_emplace(binding_range_index, std::make_unique<ComputeBuffer>(rendering_device));
				ComputeBuffer& new_buffer = *new_buffer_it->second;
				new_buffer.set_alignment(binding_range->alignment);
				new_buffer.set_size(binding_range->size);
				new_buffer.set_is_fixed_size(true);
				return &new_buffer;
			}
			case ShaderTypeLayoutShape::BindingType::TYPED_BUFFER:
			case ShaderTypeLayoutShape::BindingType::RAW_BUFFER: {
				auto [new_buffer_it, _] = buffers.try_emplace(binding_range_index, std::make_unique<ComputeBuffer>(rendering_device));
				ComputeBuffer& new_buffer = *new_buffer_it->second;
				new_buffer.set_size(256); // TODO: Default sizing behavior?
				new_buffer.set_is_fixed_size(false);
				return &new_buffer;
			}
			default:
				break;
		}
	}
	return nullptr;
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
			if (const Ref<RDUniform> uniform = data; uniform.is_valid()) {
				write_resource(uniform);
			} else {
				ERR_FAIL_NULL(shape);
				shape->write_into(*this, data);
			}
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
