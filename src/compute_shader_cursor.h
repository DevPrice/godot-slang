#pragma once

#include <map>
#include <memory>

#include "godot_cpp/classes/placeholder_texture2d.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"

#include "attributes.h"
#include "compute_buffer.h"
#include "compute_shader_shape.h"

class SamplerCache;

struct ComputeShaderOffset {
    uint32_t binding_range_offset{};
    uint32_t element_offset{};
    int64_t byte_offset{};

    ComputeShaderOffset operator+(const ComputeShaderOffset& other) const;
    ComputeShaderOffset& operator+=(const ComputeShaderOffset& other);

    static ComputeShaderOffset from_field(const FieldShape& field);
};

class ComputeShaderObject {

private:
	godot::RenderingDevice* rendering_device;
    SamplerCache* sampler_cache;
    godot::Ref<ShaderTypeLayoutShape> shape{};
    godot::PackedByteArray push_constants{};
    std::unordered_map<uint64_t, std::unique_ptr<ComputeBuffer>> buffers{};
    godot::Dictionary uniforms{};
    bool owns_binding_space{};
    int64_t first_slot_index{};
    std::unordered_map<uint64_t, std::unique_ptr<ComputeShaderObject>> subobjects{};

public:
    using DescriptorSets = std::map<uint64_t, godot::TypedArray<godot::Ref<godot::RDUniform>>>;

	ComputeShaderObject(godot::RenderingDevice* p_rendering_device, SamplerCache* p_sampler_cache, const godot::Ref<ShaderTypeLayoutShape>& p_shape, bool p_owns_binding_space = true, int64_t p_first_slot_index = 0);
    virtual ~ComputeShaderObject() = default;

    [[nodiscard]] godot::Ref<ShaderTypeLayoutShape> get_shape() const { return shape; }
    [[nodiscard]] const godot::PackedByteArray& get_push_constants() const { return push_constants; }

    void write_resource(const ComputeShaderOffset& offset, const godot::Variant& data);
    void write_bytes(const ComputeShaderOffset& offset, const godot::Variant& data, int64_t size, ShaderTypeLayoutShape::MatrixLayout matrix_layout);

    void flush_buffers();

    DescriptorSets get_descriptor_sets();
    void get_descriptor_sets(DescriptorSets& descriptor_sets, uint64_t current_space_index, uint64_t& next_space_index);
	godot::TypedArray<godot::RID> get_rids(const ComputeShaderOffset& offset) const;
	godot::PackedByteArray get_buffer_data(const ComputeShaderOffset& offset, uint32_t size_bytes = 0) const;
	godot::Error get_buffer_data_async(const godot::Callable& callback, const ComputeShaderOffset& offset, uint32_t size_bytes = 0) const;

    ComputeShaderObject* get_or_create_subobject(uint64_t binding_range_index);

private:
	std::optional<BindingRange> _get_binding_range(int64_t binding_range_index) const;
    ComputeBuffer* _get_or_create_buffer(int64_t binding_range_index);
    [[nodiscard]] godot::RID _get_resource_rid(const godot::Variant& data) const;

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
    void write(godot::Variant data) const;

	godot::TypedArray<godot::RID> get_rids() const;
	godot::PackedByteArray get_buffer_data() const;
	godot::Error get_buffer_data_async(const godot::Callable& callback) const;
};
