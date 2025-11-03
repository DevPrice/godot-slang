#include "compute_shader_shape.h"

void ComputeShaderShape::_bind_methods() {
}

void ComputeShaderVariantShape::_bind_methods() {
    BIND_GET_SET(ComputeShaderVariantShape, size, Variant::INT);
    BIND_GET_SET_ENUM(ComputeShaderVariantShape, matrix_layout, "Unknown:0,Row-major:1,Column-major:1")
}

void ComputeShaderArrayShape::_bind_methods() {
    BIND_GET_SET_RESOURCE(ComputeShaderArrayShape, element_shape, ComputeShaderShape);
    BIND_GET_SET(ComputeShaderArrayShape, size, Variant::INT);
    BIND_GET_SET(ComputeShaderArrayShape, stride, Variant::INT);
    BIND_GET_SET(ComputeShaderArrayShape, alignment, Variant::INT);
}

void ComputeShaderStructuredShape::_bind_methods() {
    BIND_GET_SET(ComputeShaderStructuredShape, size, Variant::INT);
    BIND_GET_SET(ComputeShaderStructuredShape, alignment, Variant::INT);
    BIND_GET_SET(ComputeShaderStructuredShape, properties, Variant::DICTIONARY);
    BIND_GET_SET(ComputeShaderStructuredShape, user_attributes, Variant::DICTIONARY);
}

void ComputeShaderResourceShape::_bind_methods() {
    BIND_ENUM_CONSTANT(UNKNOWN)
    BIND_ENUM_CONSTANT(RAW_BYTES)
    BIND_GET_SET_ENUM(ComputeShaderResourceShape, resource_type, "Unknown:0,Raw Bytes:1")
}

GET_SET_PROPERTY_IMPL(ComputeShaderVariantShape, ComputeShaderFile::MatrixLayout, matrix_layout)

GET_SET_PROPERTY_IMPL(ComputeShaderArrayShape, Ref<ComputeShaderShape>, element_shape)
GET_SET_PROPERTY_IMPL(ComputeShaderArrayShape, int64_t, stride)
GET_SET_PROPERTY_IMPL(ComputeShaderArrayShape, int64_t, alignment)

GET_SET_PROPERTY_IMPL(ComputeShaderStructuredShape, int64_t, alignment)
GET_SET_PROPERTY_IMPL(ComputeShaderStructuredShape, Dictionary, properties)
GET_SET_PROPERTY_IMPL(ComputeShaderStructuredShape, Dictionary, user_attributes)

GET_SET_PROPERTY_IMPL(ComputeShaderResourceShape, ComputeShaderResourceShape::ComputeShaderResourceType, resource_type)

int64_t ComputeShaderVariantShape::get_size() const { return size; }
void ComputeShaderVariantShape::set_size(const int64_t p_size) { size = p_size; }

int64_t ComputeShaderArrayShape::get_size() const { return size; }
void ComputeShaderArrayShape::set_size(const int64_t p_size) { size = p_size; }

int64_t ComputeShaderStructuredShape::get_size() const { return size; }
void ComputeShaderStructuredShape::set_size(const int64_t p_size) { size = p_size; }
