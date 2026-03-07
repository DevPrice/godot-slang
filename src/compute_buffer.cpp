#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "compute_shader_shape.h"
#include "variant_serializer.h"

#include "compute_buffer.h"

using namespace godot;

ComputeBuffer::ComputeBuffer(RenderingDevice* p_rendering_device) : rendering_device(p_rendering_device), rid(p_rendering_device) { }

void ComputeBuffer::write(const int64_t offset, const int64_t size, const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	ERR_FAIL_COND_MSG(get_is_fixed_size() && buffer.size() == 0, "Attempt to write fixed-size buffer before initialize!");
	ERR_FAIL_COND_MSG(offset + size > buffer.size(), "Attempt to write past end of buffer!");
	const VariantSerializer::Buffer serialized = VariantSerializer::serialize(data, get_is_fixed_size() ? BufferLayout::STD140 : BufferLayout::STD430, matrix_layout);
	if (serialized.compare(buffer.ptr(), size)) {
		serialized.copy(buffer.ptrw() + offset, size);
		dirty_start = Math::min(offset, dirty_start);
		dirty_end = Math::max(offset + size, dirty_end);
	}
}

void ComputeBuffer::set_size(const int64_t size) {
	ERR_FAIL_COND_MSG(get_is_fixed_size(), "Attempted to change size of fixed size buffer!");
	const int64_t alignment = get_alignment();
	if (alignment > 0) {
		buffer.resize(aligned_size(size, alignment));
	} else {
		buffer.resize(size);
	}
}

void ComputeBuffer::flush() {
	ERR_FAIL_NULL(rendering_device);

	if (!get_is_fixed_size()) {
		if (remote_size < buffer.size() && rid.is_valid()) {
			rid.reset();
			dirty_start = 0;
			dirty_end = buffer.size();
		} else if (buffer.is_empty()) {
			// the buffer cannot be 0 bytes
			set_size(256);
			dirty_start = 0;
			dirty_end = buffer.size();
		}
	}

	if (!rid.is_valid()) {
		ERR_FAIL_COND(buffer.is_empty());
		rid = get_is_fixed_size() ? rendering_device->uniform_buffer_create(buffer.size(), buffer) : rendering_device->storage_buffer_create(buffer.size(), buffer);
	} else if (dirty_start != dirty_end) {
		rendering_device->buffer_update(rid, dirty_start, Math::min(dirty_end - dirty_start, buffer.size()), buffer);
	}
	remote_size = buffer.size();
	dirty_start = 0;
	dirty_end = 0;
}

int64_t ComputeBuffer::aligned_size(const int64_t size, const int64_t alignment) {
	return alignment * ((size + (alignment - 1)) / alignment);
}

RID ComputeBuffer::get_rid() const {
	return rid;
}

GET_SET_PROPERTY_IMPL(ComputeBuffer, PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(ComputeBuffer, int64_t, alignment)
GET_SET_PROPERTY_IMPL(ComputeBuffer, bool, is_fixed_size)
