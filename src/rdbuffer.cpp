#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "rdbuffer.h"

#include "compute_shader_shape.h"
#include "variant_serializer.h"

void RDBuffer::_bind_methods() {
}

RDBuffer::~RDBuffer() {
	if (rid.is_valid()) {
		if (const RenderingServer* rendering_server = RenderingServer::get_singleton()) {
			if (RenderingDevice* rd = rendering_server->get_rendering_device()) {
				rd->free_rid(rid);
			}
		}
	}
}

void RDBuffer::write(const int64_t offset, const int64_t size, const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	ERR_FAIL_COND_MSG(get_is_fixed_size() && buffer.size() == 0, "Writing fixed-size buffer before initialize!");
	const VariantSerializer::Buffer serialized = VariantSerializer::serialize(data, matrix_layout);
	if (serialized.compare(buffer.ptr(), size)) {
		serialized.copy(buffer.ptrw() + offset, size);
		dirty_start = Math::min(offset, dirty_start);
		dirty_end = Math::max(offset + size, dirty_end);
	}
}

void RDBuffer::set_size(const int64_t size) {
	ERR_FAIL_COND_MSG(get_is_fixed_size(), "Attempted to change size of fixed size buffer!");
	const int64_t alignment = get_alignment();
	if (alignment > 0) {
		buffer.resize(aligned_size(size, alignment));
	} else {
		buffer.resize(size);
	}
}

void RDBuffer::flush() {
	RenderingServer* rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL(rendering_server);
	ERR_FAIL_COND(!rendering_server->is_on_render_thread());
	RenderingDevice* rd = rendering_server->get_rendering_device();
	ERR_FAIL_NULL(rd);

	if (!get_is_fixed_size()) {
		if (remote_size < buffer.size() && rid.is_valid()) {
			// the buffer grew in size, we need to recreate it
			rd->free_rid(rid);
			set_rid(RID());
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
		set_rid(get_is_fixed_size() ? rd->uniform_buffer_create(buffer.size(), buffer) : rd->storage_buffer_create(buffer.size(), buffer));
	} else if (dirty_start != dirty_end) {
		rd->buffer_update(rid, dirty_start, Math::min(dirty_end - dirty_start, buffer.size()), buffer);
	}
	remote_size = buffer.size();
	dirty_start = 0;
	dirty_end = 0;
}

RenderingDevice::UniformType RDBuffer::get_uniform_type() const {
	return get_is_fixed_size() ? RenderingDevice::UNIFORM_TYPE_UNIFORM_BUFFER : RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
}

int64_t RDBuffer::aligned_size(const int64_t size, const int64_t alignment) {
	return alignment * ((size + (alignment - 1)) / alignment);
}

GET_SET_PROPERTY_IMPL(RDBuffer, RID, rid)
GET_SET_PROPERTY_IMPL(RDBuffer, PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(RDBuffer, int64_t, alignment)
GET_SET_PROPERTY_IMPL(RDBuffer, bool, is_fixed_size)
