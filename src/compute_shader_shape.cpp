#include "compute_shader_shape.h"

#include "compute_shader_file.h"
#include "enums.h"

void ShaderTypeLayoutShape::_bind_methods() {
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

GET_SET_PROPERTY_IMPL(VariantTypeLayoutShape, ShaderTypeLayoutShape::MatrixLayout, matrix_layout)

GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, Ref<ShaderTypeLayoutShape>, element_shape)
GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, int64_t, stride)
GET_SET_PROPERTY_IMPL(ArrayTypeLayoutShape, int64_t, alignment)

GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, int64_t, alignment)
GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, Dictionary, properties)
GET_SET_PROPERTY_IMPL(StructTypeLayoutShape, Dictionary, user_attributes)

GET_SET_PROPERTY_IMPL(ResourceTypeLayoutShape, ResourceTypeLayoutShape::ComputeShaderResourceType, resource_type)
GET_SET_PROPERTY_IMPL(ResourceTypeLayoutShape, RenderingDevice::UniformType, uniform_type)

int64_t VariantTypeLayoutShape::get_size() const { return size; }
void VariantTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }

int64_t ArrayTypeLayoutShape::get_size() const { return size; }
void ArrayTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }

int64_t StructTypeLayoutShape::get_size() const { return size; }
void StructTypeLayoutShape::set_size(const int64_t p_size) { size = p_size; }
