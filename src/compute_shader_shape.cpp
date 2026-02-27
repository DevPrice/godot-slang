#include "compute_shader_shape.h"

#include "compute_shader_file.h"
#include "enums.h"
#include "compute_shader_cursor.h"
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
    BIND_GET_SET(ArrayTypeLayoutShape, alignment, Variant::INT);
}

void StructPropertyShape::_bind_methods() {
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
GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, int64_t, alignment)

GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, int64_t, alignment)
GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, Dictionary, properties)
GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, Dictionary, user_attributes)

GET_SET_PROPERTY_IMPL(ResourceTypeLayoutShape, ResourceTypeLayoutShape::ComputeShaderResourceType, resource_type)
GET_SET_PROPERTY_IMPL(ResourceTypeLayoutShape, RenderingDevice::UniformType, uniform_type)

void ResourceTypeLayoutShape::write_into(const ComputeShaderCursor& cursor, const Variant& data) const {
    if (get_resource_type() == RAW_BYTES) {
        const VariantSerializer::Buffer buffer = VariantSerializer::serialize(data);
        const PackedByteArray bytes = buffer.as_packed_byte_array();
        cursor.write_bytes(bytes, bytes.size());
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
    Variant key;
    bool is_valid;
    if (data.iter_init(key, is_valid) && is_valid) {
        int64_t i = 0;
        do {
            Variant value = data.iter_get(key, is_valid);
            if (is_valid) {
                cursor.element(i++).write(value);
            }
        } while (data.iter_next(key, is_valid) && is_valid);
    }
}

int64_t StructTypeLayoutShape::get_size() const { return size; }

void StructTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }

std::optional<Dictionary> StructTypeLayoutShape::field(const StringName& field_name) const {
	ERR_FAIL_COND_V(!properties.has(field_name), std::nullopt);
    return properties[field_name];
}

void StructTypeLayoutShape::write_into(const ComputeShaderCursor& cursor, const Variant& data) const {
    const Dictionary properties = get_properties();
    for (const StringName property_name : properties.keys()) {
        const Dictionary property = properties[property_name];
        bool is_valid{};
        Variant property_value = data.get_named(property_name, is_valid);
        cursor.field(property_name).write(property_value);
    }
}
