#pragma once

#include "attributes.h"
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

class SamplerCache {

private:
    RenderingDevice* rd;
    TypedArray<RID> cache{};

public:
    explicit SamplerCache(RenderingDevice* p_rendering_device);
    virtual ~SamplerCache();

    RID get_sampler(const Ref<RDSamplerState>& sampler_state);
};

class ComputeShaderObject {

private:
    RenderingDevice* rd;
    SamplerCache* sampler_cache;
    Ref<StructTypeLayoutShape> shape{};
    PackedByteArray push_constants{};
    Dictionary buffers{};
    TypedArray<Array> uniforms{};

public:
    ComputeShaderObject(RenderingDevice* p_rendering_device, SamplerCache* p_sampler_cache, const Ref<StructTypeLayoutShape>& p_shape);
    virtual ~ComputeShaderObject() = default;

    [[nodiscard]] Ref<ShaderTypeLayoutShape> get_shape() const { return shape; }

    void write_resource(const ComputeShaderOffset& offset, const Variant& data);
    void write(const ComputeShaderOffset& offset, const Variant& data, int64_t size, ShaderTypeLayoutShape::MatrixLayout matrix_layout);

    void flush_buffers();
    void bind_uniforms(int64_t compute_list, const RID& shader_rid) const;

private:
    RDBuffer& _get_buffer(int64_t binding_space, int64_t binding_index);
    RDUniform& _get_uniform(int64_t binding_space, int64_t binding_index);
    [[nodiscard]] RID _get_resource_rid(const Variant& data) const;

    [[nodiscard]] static Variant _get_default_value(RenderingDevice::UniformType type);
};

class ComputeShaderCursor {

private:
    struct WriteHandlerWithPriority {
        AttributeRegistry::WriteHandler handler{};
        int64_t priority{};

        bool operator<(const WriteHandlerWithPriority& other) const {
            return priority > other.priority;
        }
    };

    ComputeShaderOffset offset{};
    ComputeShaderObject* object;
    Ref<ShaderTypeLayoutShape> shape{};

    std::multiset<WriteHandlerWithPriority> write_handlers{};
    Variant default_value{};

public:
    explicit ComputeShaderCursor(ComputeShaderObject* p_object) : object(p_object) {
        if (object) {
            shape = object->get_shape();
        }
    }

    [[nodiscard]] ComputeShaderCursor field(const StringName& path) const;
    [[nodiscard]] ComputeShaderCursor element(int64_t index) const;

    void write_bytes(const Variant& data, int64_t size, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) const;
    void write_resource(const Variant& data) const;
    void write(const Variant& data) const;

private:
    Variant _apply_write_handlers(Variant data) const;

};
