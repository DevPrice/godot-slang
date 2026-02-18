#pragma once

#include "godot_cpp/classes/ref_counted.hpp"

#include "binding_macros.h"
#include "compute_shader_file.h"
#include "compute_shader_shape.h"

using namespace godot;

class RDBuffer : public RefCounted {
    GDCLASS(RDBuffer, RefCounted);

    GET_SET_PROPERTY(RID, rid)
    GET_SET_PROPERTY(PackedByteArray, buffer)
    GET_SET_PROPERTY(int64_t, alignment)
    GET_SET_PROPERTY(bool, is_fixed_size)

protected:
    static void _bind_methods();

public:
    RDBuffer();
    ~RDBuffer() override;

    void write(int64_t offset, int64_t size, const Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
    void set_size(int64_t size);
    void flush();
    [[nodiscard]] RenderingDevice::UniformType get_uniform_type() const;

    static Ref<RDBuffer> ref(const RID& buffer_rid);
    static int64_t aligned_size(int64_t size, int64_t alignment);

private:
    bool is_ref = false;
    int64_t remote_size = 0;
    int64_t dirty_start = 0;
    int64_t dirty_end = 0;
};