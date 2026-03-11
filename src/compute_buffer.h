#pragma once

#include "binding_macros.h"
#include "compute_shader_shape.h"
#include "rids.h"

enum class ComputeBufferType {
	CONSTANT_BUFFER,
	TEXTURE_BUFFER,
	STORAGE_BUFFER,
};

class ComputeBuffer {

    GET_SET_PROPERTY(godot::PackedByteArray, buffer)
    GET_SET_PROPERTY(int64_t, alignment)
    GET_SET_PROPERTY(bool, is_fixed_size)

public:
	ComputeBuffer(godot::RenderingDevice* p_rendering_device, ComputeBufferType p_type);

	godot::RID get_rid() const;

	void write(int64_t offset, std::span<const uint8_t> data);
    void write(int64_t offset, int64_t size, const godot::Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
    void set_size(int64_t size);
    void flush();

    static int64_t aligned_size(int64_t size, int64_t alignment);

private:
	godot::RenderingDevice* rendering_device;
	UniqueRID<godot::RenderingDevice> rid;
	ComputeBufferType type;
    int64_t remote_size = 0;
    int64_t dirty_start = 0;
    int64_t dirty_end = 0;

	godot::RID _create_buffer();
	void _update_buffer();
};