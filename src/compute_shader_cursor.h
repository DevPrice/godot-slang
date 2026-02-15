#pragma once

#include "compute_shader_shape.h"
#include "rdbuffer.h"

struct ComputeShaderOffset {
    uint32_t binding_offset{};
    uint32_t binding_index_offset{};
    int64_t byte_offset{};

    ComputeShaderOffset operator+(const ComputeShaderOffset& other) const;
    ComputeShaderOffset& operator+=(const ComputeShaderOffset& other);

    static ComputeShaderOffset from_field(const Dictionary& field);
};

class ComputeShaderObject {

private:
    RenderingDevice* rd;
    Ref<StructTypeLayoutShape> shape{};
    PackedByteArray push_constants{};
    Dictionary buffers{};
    TypedArray<Array> uniforms{};
	TypedArray<RID> sampler_cache{};

public:
    ComputeShaderObject(RenderingDevice* p_rendering_device, const Ref<StructTypeLayoutShape>& p_shape, const TypedArray<Dictionary>& buffer_info);

    [[nodiscard]] Ref<ShaderTypeLayoutShape> get_shape() const { return shape; }

    void write_resource(ComputeShaderOffset offset, const Variant& data);
    void write(ComputeShaderOffset offset, const Variant& data, int64_t size, ShaderTypeLayoutShape::MatrixLayout matrix_layout);

    void flush_buffers();
    void bind_uniforms(int64_t compute_list, const RID& shader_rid) const;

private:
    RDBuffer& _get_buffer(int64_t binding_space, int64_t binding_index);
    RDUniform& _get_uniform(int64_t binding_space, int64_t binding_index);
    RID _get_resource_rid(const Variant& data);
    RID _get_sampler(RenderingDevice::SamplerFilter filter, RenderingDevice::SamplerRepeatMode repeat_mode);

    [[nodiscard]] static Variant _get_default_value(RenderingDevice::UniformType type);
};

class ComputeShaderCursor {

private:
    ComputeShaderOffset offset{};
    ComputeShaderObject* object;
    Ref<ShaderTypeLayoutShape> shape{};

public:
    explicit ComputeShaderCursor(ComputeShaderObject* p_object) : object(p_object) {
        if (object) {
            shape = object->get_shape();
        }
    }

    [[nodiscard]] ComputeShaderCursor field(const StringName& path) const;
    [[nodiscard]] ComputeShaderCursor element(int64_t index) const;
    void write(const Variant& data) const;

private:
    [[nodiscard]] static Variant _get_default_value(Dictionary property);

};
