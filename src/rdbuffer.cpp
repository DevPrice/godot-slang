#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "rdbuffer.h"

void RDBuffer::_bind_methods() {
}

RDBuffer::RDBuffer() {
	alignment = 16;
}

RDBuffer::~RDBuffer() {
	if (!is_ref && !rid.is_valid()) {
		RenderingServer::get_singleton()->free_rid(rid);
	}
}

void RDBuffer::write(const int64_t offset, const int64_t size, const Variant& data) {
	if (get_is_fixed_size()) {
		ERR_FAIL_COND_MSG(buffer.size() == 0, "Writing fixed-size buffer before initialize!");
	} else {
		ERR_FAIL_COND_MSG(get_stride() == 0, "Attempted to write storage buffer without stride!");
	}
	dirty_start = Math::min(offset, dirty_start);
	dirty_end = Math::max(offset + size, dirty_end);
	switch (data.get_type()) {
		case Variant::PACKED_BYTE_ARRAY: {
			buffer_copy<PackedByteArray>(data, offset, size);
			break;
		}
		case Variant::PACKED_INT32_ARRAY: {
			buffer_copy<PackedInt32Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_INT64_ARRAY: {
			buffer_copy<PackedInt64Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			buffer_copy<PackedFloat32Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			buffer_copy<PackedFloat64Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			buffer_copy<PackedVector2Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			buffer_copy<PackedVector3Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			buffer_copy<PackedVector4Array>(data, offset, size);
			break;
		}
		case Variant::PACKED_COLOR_ARRAY: {
			buffer_copy<PackedColorArray>(data, offset, size);
			break;
		}
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray string_array = data;
			buffer_copy(string_array.to_byte_array(), offset, size);
			break;
		}
		case Variant::ARRAY: {
			const int64_t element_stride = get_stride();
			ERR_FAIL_COND_MSG(element_stride == 0, "Cannot write array to buffer if stride is unset!");
			const Array array = data;
			int64_t element_offset = offset;
			const int64_t array_size_bytes = array.size() * element_stride;
			if (!get_is_fixed_size() && buffer.size() < array_size_bytes) {
				set_size(array_size_bytes);
			}
			for (const Variant& element : array) {
				write(element_offset, element_stride, element);
				element_offset += element_stride;
			}
		}
		default:
			write(buffer, offset, size, data);
			break;
	}
}

void RDBuffer::set_size(const int64_t size) {
	ERR_FAIL_COND_MSG(is_ref, "Attempted to change size of ref buffer!");
	ERR_FAIL_COND_MSG(get_is_fixed_size(), "Attempted to change size of fixed size buffer!");
	const int64_t alignment = get_alignment();
	if (alignment > 0) {
		const int64_t aligned_size = alignment * ((size + (alignment - 1)) / alignment);
		buffer.resize(aligned_size);
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
	ERR_FAIL_COND(buffer.is_empty());

	if (!rid.is_valid()) {
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

Ref<RDBuffer> RDBuffer::ref(const RID& buffer_rid) {
	Ref result = memnew(RDBuffer);
	result->set_rid(buffer_rid);
	result->set_is_fixed_size(true);
	result->is_ref = true;
	return result;
}

void RDBuffer::write(PackedByteArray& destination, const int64_t offset, const int64_t size, const Variant& data) {
	switch (data.get_type()) {
		case Variant::BOOL:
			destination.encode_s32(offset, data ? 1 : 0);
			break;
		case Variant::INT:
			destination.encode_s32(offset, data);
			break;
		case Variant::FLOAT:
			destination.encode_float(offset, data);
			break;
		case Variant::VECTOR2: {
			const Vector2 vector = data;
			destination.encode_float(offset, vector.x);
			destination.encode_float(offset + 4, vector.y);
			break;
		}
		case Variant::VECTOR3: {
			const Vector3 vector = data;
			destination.encode_float(offset, vector.x);
			destination.encode_float(offset + 4, vector.y);
			destination.encode_float(offset + 8, vector.z);
			break;
		}
		case Variant::VECTOR4: {
			const Vector4 vector = data;
			destination.encode_float(offset, vector.x);
			destination.encode_float(offset + 4, vector.y);
			destination.encode_float(offset + 8, vector.z);
			destination.encode_float(offset + 12, vector.w);
			break;
		}
		case Variant::COLOR: {
			const Color color = data;
			destination.encode_float(offset, color.r);
			destination.encode_float(offset + 4, color.g);
			destination.encode_float(offset + 8, color.b);
			if (size >= 16) {
				destination.encode_float(offset + 12, color.a);
			}
			break;
		}
		case Variant::VECTOR2I: {
			const Vector2i vector = data;
			destination.encode_s32(offset, vector.x);
			destination.encode_s32(offset + 4, vector.y);
			break;
		}
		case Variant::VECTOR3I: {
			const Vector3i vector = data;
			destination.encode_s32(offset, vector.x);
			destination.encode_s32(offset + 4, vector.y);
			destination.encode_s32(offset + 8, vector.z);
			break;
		}
		case Variant::VECTOR4I: {
			const Vector4i vector = data;
			destination.encode_s32(offset, vector.x);
			destination.encode_s32(offset + 4, vector.y);
			destination.encode_s32(offset + 8, vector.z);
			destination.encode_s32(offset + 12, vector.w);
			break;
		}
		default:
			break;
	}
}

GET_SET_PROPERTY_IMPL(RDBuffer, RID, rid)
GET_SET_PROPERTY_IMPL(RDBuffer, PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(RDBuffer, int64_t, stride)
GET_SET_PROPERTY_IMPL(RDBuffer, int64_t, alignment)
GET_SET_PROPERTY_IMPL(RDBuffer, bool, is_fixed_size)
