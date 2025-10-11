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
	ERR_FAIL_COND_MSG(get_is_fixed_size() && buffer.size() == 0, "Writing fixed-size buffer before initialize!");
	dirty_start = Math::min(offset, dirty_start);
	dirty_end = Math::max(offset + size, dirty_end);
	write(buffer, offset, size, data);
}

void RDBuffer::write_shape(const int64_t offset, const Dictionary& shape, const Variant& data) {
	const int64_t size = write_shape(buffer, offset, shape, data, !get_is_fixed_size());
	dirty_start = Math::min(offset, dirty_start);
	dirty_end = Math::max(offset + size, dirty_end);
}

void RDBuffer::set_size(const int64_t size) {
	ERR_FAIL_COND_MSG(is_ref, "Attempted to change size of ref buffer!");
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

Ref<RDBuffer> RDBuffer::ref(const RID& buffer_rid) {
	Ref result = memnew(RDBuffer);
	result->set_rid(buffer_rid);
	result->set_is_fixed_size(true);
	result->is_ref = true;
	return result;
}

void RDBuffer::write(PackedByteArray& destination, const int64_t offset, const int64_t size, const Variant& data) {
	ERR_FAIL_COND(offset < 0);
	ERR_FAIL_COND(offset + size > destination.size());
	switch (data.get_type()) {
		case Variant::NIL:
			memset(destination.ptrw() + offset, 0, size);
			break;
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
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray& array = data;
			memcpy(destination.ptrw() + offset, array.ptr(), Math::min(array.size(), size));
			break;
		}
		default:
			// TODO: Should handle all of the other math types
			// TODO: Maybe handle some other types
			ERR_FAIL_MSG(String("Unsupported data type: ") + Variant::get_type_name(data.get_type()));
	}
}

int64_t RDBuffer::write_shape(PackedByteArray& destination, const int64_t offset, const Dictionary& shape, const Variant& data, const bool resize) {
	static const StringName type_structured = "structured";
	static const StringName type_array = "array";
	static const StringName type_simple = "simple";
	const String type = shape["type"];
	if (type == type_simple) {
		const int64_t size = shape["size"];
		ERR_FAIL_COND_V(size <= 0, 0);
		write(destination, offset, size, data);
		return size;
	}
	if (type == type_structured) {
		const Dictionary properties = shape["properties"];
		int64_t property_offset = offset;
		for (const StringName property_name : properties.keys()) {
			const Dictionary property = properties[property_name];
			const Dictionary property_shape = property["shape"];
			const int64_t property_size = property_shape["size"];
			const int64_t alignment = shape["alignment"];
			ERR_FAIL_COND_V(property_size == 0, property_offset - offset);

			bool is_valid{};
			const Variant property_value = data.get_named(property_name, is_valid);
			if (is_valid) {
				write_shape(destination, property_offset, property_shape, property_value, resize);
			}
			property_offset = aligned_size(property_offset + property_size, alignment);
		}
		return property_offset - offset;
	}
	if (type == type_array) {
		const Dictionary element_shape = shape["element_shape"];

		const int64_t stride = shape["stride"];
		ERR_FAIL_COND_V(stride <= 0, 0);
		const int64_t size = shape["size"];
		if (resize && destination.size() < offset + size) {
			destination.resize(offset + size);
		}

		int64_t element_offset = offset;

		Variant key;
		bool is_valid;
		if (data.iter_init(key, is_valid) && is_valid) {
			do {
				Variant value = data.iter_get(key, is_valid);
				if (is_valid) {
					write_shape(destination, element_offset, element_shape, value, resize);
					element_offset += stride;
				}
			} while (data.iter_next(key, is_valid) && is_valid);
		}
		return element_offset - offset;
	}
	UtilityFunctions::push_warning("Unable to write invalid shape: ", shape);
	return 0;
}

int64_t RDBuffer::aligned_size(const int64_t size, const int64_t alignment) {
	return alignment * ((size + (alignment - 1)) / alignment);
}

GET_SET_PROPERTY_IMPL(RDBuffer, RID, rid)
GET_SET_PROPERTY_IMPL(RDBuffer, PackedByteArray, buffer)
GET_SET_PROPERTY_IMPL(RDBuffer, int64_t, alignment)
GET_SET_PROPERTY_IMPL(RDBuffer, bool, is_fixed_size)
