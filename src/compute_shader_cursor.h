#pragma once

#include <map>
#include <memory>

#include "godot_cpp/classes/placeholder_texture2d.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"

#include "attributes.h"
#include "compute_shader_shape.h"

class RDBuffer;

struct ComputeShaderOffset {
    uint32_t binding_range_offset{};
    uint32_t binding_index_offset{};
    int64_t byte_offset{};

    ComputeShaderOffset operator+(const ComputeShaderOffset& other) const;
    ComputeShaderOffset& operator+=(const ComputeShaderOffset& other);

    static ComputeShaderOffset from_field(const godot::Dictionary& field);
};

class SamplerCache {

private:
    godot::RenderingDevice* rd;
    godot::TypedArray<godot::RID> cache{};

public:
    explicit SamplerCache(godot::RenderingDevice* p_rendering_device);
    virtual ~SamplerCache();

    godot::RID get_sampler(const godot::Ref<godot::RDSamplerState>& sampler_state);
};

class ComputeShaderObject {

private:
    SamplerCache* sampler_cache;
    godot::Ref<ShaderTypeLayoutShape> shape{};
    godot::PackedByteArray push_constants{};
    godot::Dictionary buffers{};
    godot::TypedArray<godot::Ref<godot::RDUniform>> uniforms{};
    bool owns_binding_space{};
    std::unordered_map<uint64_t, std::unique_ptr<ComputeShaderObject>> subobjects{};

public:
    using DescriptorSets = std::map<uint64_t, godot::TypedArray<godot::Ref<godot::RDUniform>>>;

    ComputeShaderObject(SamplerCache* p_sampler_cache, const godot::Ref<ShaderTypeLayoutShape>& p_shape, bool p_owns_binding_space = true);
    virtual ~ComputeShaderObject() = default;

    [[nodiscard]] godot::Ref<ShaderTypeLayoutShape> get_shape() const { return shape; }

    void write_resource(const ComputeShaderOffset& offset, const godot::Variant& data);
    void write(const ComputeShaderOffset& offset, const godot::Variant& data, int64_t size, ShaderTypeLayoutShape::MatrixLayout matrix_layout);

    void flush_buffers();

    DescriptorSets get_descriptor_sets() const;
    const godot::PackedByteArray& get_push_constants() const { return push_constants; }

    ComputeShaderObject* get_or_create_subobject(uint64_t binding_range_index);

private:
    RDBuffer& _get_buffer(int64_t binding_index);
    godot::RDUniform& _get_uniform(int64_t binding_index);
    [[nodiscard]] godot::RID _get_resource_rid(const godot::Variant& data) const;

    void get_descriptor_sets(DescriptorSets& descriptor_sets, uint64_t current_space_index, uint64_t& next_space_index) const;

    [[nodiscard]] static godot::Variant _get_default_value(godot::RenderingDevice::UniformType type);
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
    godot::Ref<ShaderTypeLayoutShape> shape{};
    const godot::Object* dispatch_context;

    std::multiset<WriteHandlerWithPriority> write_handlers{};
    godot::Variant default_value{};

public:
    explicit ComputeShaderCursor(ComputeShaderObject* p_object, const godot::Object* p_context = nullptr)
        : object(p_object), shape(object ? object->get_shape() : nullptr), dispatch_context(p_context) {}

    [[nodiscard]] ComputeShaderCursor field(const godot::StringName& path) const;
    [[nodiscard]] ComputeShaderCursor element(int64_t index) const;

    void write_bytes(const godot::Variant& data, int64_t size, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) const;
    void write_resource(const godot::Variant& data) const;
    void write(const godot::Variant& data) const;

private:
    godot::Variant _apply_write_handlers(godot::Variant data) const;

};
