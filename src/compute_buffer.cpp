#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "compute_shader_shape.h"
#include "variant_serializer.h"

#include "compute_buffer.h"

using namespace godot;

ComputeBuffer::ComputeBuffer(RenderingDevice* p_rendering_device, const ComputeBufferType p_type) : rendering_device(p_rendering_device), rid(p_rendering_device), type(p_type) { }

void ComputeBuffer::write(const int64_t offset, const std::span<const uint8_t> data) {
	ERR_FAIL_COND_MSG(get_is_fixed_size() && buffer.size() == 0, "Attempt to write fixed-size buffer before initialize!");
	ERR_FAIL_COND_MSG(offset + data.size() > buffer.size(), "Attempt to write past end of buffer!");
	const int64_t size = Math::min<size_t>(data.size(), buffer.size() - offset);
	if (memcmp(buffer.ptr(), data.data(), size)) {
		memcpy(buffer.ptrw() + offset, data.data(), size);
		dirty_start = Math::min(offset, dirty_start);
		dirty_end = Math::max<int64_t>(offset + data.size(), dirty_end);
	}
}

void ComputeBuffer::write(const int64_t offset, const int64_t size, const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	const VariantSerializer::Buffer serialized = VariantSerializer::serialize(data, get_is_fixed_size() ? BufferLayout::STD140 : BufferLayout::STD430, matrix_layout);
	const std::span<const uint8_t> span = serialized;
	if (span.size() > size) {
		write(offset, span.subspan(0, size));
	} else {
		write(offset, span);
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
		rid = _create_buffer();
	} else if (dirty_start != dirty_end) {
		_update_buffer();
	}
	remote_size = buffer.size();
	dirty_start = 0;
	dirty_end = 0;
}

int64_t ComputeBuffer::aligned_size(const int64_t size, const int64_t alignment) {
	return alignment * ((size + (alignment - 1)) / alignment);
}

RID ComputeBuffer::_create_buffer() {
	switch (type) {
		case ComputeBufferType::CONSTANT_BUFFER:
			return rendering_device->uniform_buffer_create(buffer.size(), buffer);
		case ComputeBufferType::STORAGE_BUFFER:
			return rendering_device->storage_buffer_create(buffer.size(), buffer);
		case ComputeBufferType::TEXTURE_BUFFER:
			// TODO: Format?
			return rendering_device->texture_buffer_create(buffer.size() / 16, RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT, buffer);
	}
	ERR_FAIL_V_MSG({}, String("Invalid buffer type %s!") % static_cast<int64_t>(type));
}

void ComputeBuffer::_update_buffer() {
	if (type == ComputeBufferType::TEXTURE_BUFFER) {
		// TODO: buffer_update and texture_update both fail?
		rid = rendering_device->texture_buffer_create(buffer.size() / 16, RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT, buffer);
	} else {
		rendering_device->buffer_update(rid, dirty_start, Math::min(dirty_end - dirty_start, buffer.size()), buffer);
	}
}

RID ComputeBuffer::get_rid() const {
	return rid;
}

GET_SET_PROPERTY_IMPL(ComputeBuffer, PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(ComputeBuffer, int64_t, alignment)
GET_SET_PROPERTY_IMPL(ComputeBuffer, bool, is_fixed_size)
