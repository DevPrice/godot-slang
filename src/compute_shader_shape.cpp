#include "compute_shader_shape.h"

#include "compute_shader_cursor.h"
#include "compute_shader_file.h"
#include "enums.h"
#include "variant_serializer.h"

using namespace godot;

void ShaderTypeLayoutShape::_bind_methods() {
    BIND_GET_SET(ShaderTypeLayoutShape, bindings, Variant::ARRAY);
}

void VariantTypeLayoutShape::_bind_methods() {
    BIND_GET_SET(VariantTypeLayoutShape, size, Variant::INT);
    BIND_GET_SET_ENUM(VariantTypeLayoutShape, matrix_layout, "Unknown:0,Row-major:1,Column-major:1")
}

void ArrayTypeLayoutShape::_bind_methods() {
    BIND_GET_SET_RESOURCE(ArrayTypeLayoutShape, element_shape, ShaderTypeLayoutShape);
    BIND_GET_SET(ArrayTypeLayoutShape, size, Variant::INT);
    BIND_GET_SET(ArrayTypeLayoutShape, stride, Variant::INT);
    BIND_GET_SET(ArrayTypeLayoutShape, element_count, Variant::INT);
}

void StructTypeLayoutShape::_bind_methods() {
    BIND_GET_SET(StructTypeLayoutShape, size, Variant::INT);
    BIND_GET_SET(StructTypeLayoutShape, alignment, Variant::INT);
    BIND_GET_SET(StructTypeLayoutShape, properties, Variant::DICTIONARY);
    BIND_GET_SET(StructTypeLayoutShape, user_attributes, Variant::DICTIONARY);
}

void ResourceTypeLayoutShape::_bind_methods() {
    BIND_ENUM_CONSTANT(UNKNOWN)
    BIND_ENUM_CONSTANT(RAW_BYTES)
    BIND_GET_SET_ENUM(ResourceTypeLayoutShape, resource_type, ENUM_HINT_STRING(ResourceTypeLayoutShape, ComputeShaderResourceType))
    BIND_GET_SET_ENUM(ResourceTypeLayoutShape, uniform_type, ENUM_HINT_STRING(RenderingDevice, UniformType))
}

GET_SET_PROPERTY_IMPL(ShaderTypeLayoutShape, TypedArray<Dictionary>, bindings)

GET_SET_PROPERTY_IMPL(VariantTypeLayoutShape, ShaderTypeLayoutShape::MatrixLayout, matrix_layout)

GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, Ref<ShaderTypeLayoutShape>, element_shape)
GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, int64_t, stride)
GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, int64_t, element_count)

GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, int64_t, alignment)
GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, Dictionary, properties)
GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, Dictionary, user_attributes)

GET_SET_PROPERTY_IMPL(ResourceTypeLayoutShape, ResourceTypeLayoutShape::ComputeShaderResourceType, resource_type)
GET_SET_PROPERTY_IMPL(ResourceTypeLayoutShape, RenderingDevice::UniformType, uniform_type)

void ResourceTypeLayoutShape::write_into(const ComputeShaderCursor& cursor, const Variant& data) const {
    if (get_resource_type() == RAW_BYTES) {
        const VariantSerializer::Buffer buffer = VariantSerializer::serialize(data, BufferLayout::STD430);
        cursor.write_bytes(buffer);
    } else {
        cursor.write_resource(data);
    }
}

int64_t VariantTypeLayoutShape::get_size() const { return size; }
void VariantTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }

void VariantTypeLayoutShape::write_into(const ComputeShaderCursor& cursor, const Variant& data) const {
	ERR_FAIL_COND(get_size() <= 0);
    cursor.write_bytes(data, get_size(), get_matrix_layout());
}

int64_t ArrayTypeLayoutShape::get_size() const { return size; }
void ArrayTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }

void ArrayTypeLayoutShape::write_into(const ComputeShaderCursor& cursor, const Variant& data) const {
	// TODO: Maybe these should be separate shapes
	if (get_element_count()) {
		// fixed-size array
		for (int64_t i = 0; i < get_element_count(); ++i) {
			bool is_valid{}, oob{};
			const Variant value = data.get_indexed(i, is_valid, oob);
			cursor.element(i).write(value);
		}
	} else {
		// structured buffer, unbounded size
		Variant key;
		bool is_valid;
		if (data.iter_init(key, is_valid) && is_valid) {
			int64_t i = 0;
			do {
				const Variant value = data.iter_get(key, is_valid);
				if (is_valid) {
					cursor.element(i++).write(value);
				}
			} while (data.iter_next(key, is_valid) && is_valid);
		} else {
			// TODO: Is there a better way to handle an empty buffer param?
			// this will ensure a buffer is at least created with the default sizing
			cursor.write_bytes(std::span<const uint8_t>());
		}
	}
}

#define STR_NAME_KEY(name) const StringName& key_##name() { \
static StringName key(#name); \
return key; \
}

STR_NAME_KEY(name)
STR_NAME_KEY(shape)
STR_NAME_KEY(user_attributes)
STR_NAME_KEY(binding_offset)
STR_NAME_KEY(offset)
STR_NAME_KEY(default_value)
STR_NAME_KEY(property_info)

STR_NAME_KEY(binding_type)
STR_NAME_KEY(uniform_type)
STR_NAME_KEY(slot_offset)
STR_NAME_KEY(binding_count)
STR_NAME_KEY(size)
STR_NAME_KEY(alignment)
STR_NAME_KEY(leaf_shape)

#undef STR_NAME_KEY

FieldShape::operator Dictionary() const {
	Dictionary result;
	result[key_name()] = name;
	result[key_shape()] = shape;
	result[key_user_attributes()] = user_attributes;
	result[key_binding_offset()] = binding_offset;
	result[key_offset()] = byte_offset;
	if (default_value.get_type() != Variant::NIL) {
		result[key_default_value()] = default_value;
	}
	if (property_info) {
		result[key_property_info()] = Dictionary(*property_info);
	}
	return result;
}

FieldShape FieldShape::from_dict(const Dictionary& dict) {
	return FieldShape{
		dict.get(key_name(), {}),
		dict.get(key_shape(), {}),
		dict.get(key_user_attributes(), {}),
		dict.get(key_default_value(), {}),
		dict.has(key_property_info())
			? std::make_optional(PropertyInfo::from_dict(dict[key_property_info()]))
			: std::nullopt,
		dict.get(key_binding_offset(), {}),
		dict.get(key_offset(), {}),
	};
}

BindingRange::operator Dictionary() const {
	Dictionary result;
	result[key_binding_type()] = static_cast<int64_t>(type);
	if (uniform_type) {
		result[key_uniform_type()] = *uniform_type;
	}
	result[key_slot_offset()] = slot_offset;
	result[key_binding_count()] = binding_count;
	if (size > 0) {
		result[key_size()] = size;
	}
	if (alignment > 0) {
		result[key_alignment()] = alignment;
	}
	if (leaf_shape.is_valid()) {
		result[key_leaf_shape()] = leaf_shape;
	}
	return result;
}

BindingRange BindingRange::from_dict(const Dictionary& dict) {
	return BindingRange{
		static_cast<ShaderTypeLayoutShape::BindingType>(static_cast<int64_t>(dict.get(key_binding_type(), 0))),
		static_cast<RenderingDevice::UniformType>(static_cast<int64_t>(dict.get(key_uniform_type(), 0))),
		dict.get(key_slot_offset(), 0),
		dict.get(key_binding_count(), 1),
		dict.get(key_size(), 0),
		dict.get(key_alignment(), 1),
		dict.get(key_leaf_shape(), {}),
	};
}

int64_t StructTypeLayoutShape::get_size() const { return size; }

void StructTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }

std::optional<FieldShape> StructTypeLayoutShape::field(const StringName& field_name) const {
	if (properties.has(field_name)) {
		return FieldShape::from_dict(properties[field_name]);
	}
	return std::nullopt;
}

void StructTypeLayoutShape::write_into(const ComputeShaderCursor& cursor, const Variant& data) const {
    const Dictionary fields = get_properties();
    for (const StringName field_name : fields.keys()) {
        const FieldShape field = FieldShape::from_dict(fields[field_name]);
        bool is_valid{};
		const Variant field_value = data.get_named(field.name, is_valid);
        cursor.field(field_name).write(field_value);
    }
}
