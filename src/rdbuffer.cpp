#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rd_sampler_state.hpp"
#include "godot_cpp/classes/rd_uniform.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#include "rdbuffer.h"

#include "attributes.h"
#include "compute_shader_shape.h"

inline bool get_container_size(const Variant &v, int64_t &size) {
	switch (v.get_type()) {
		case Variant::ARRAY: {
			const Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_INT32_ARRAY: {
			const PackedInt32Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_INT64_ARRAY: {
			const PackedInt64Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			const PackedVector2Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			const PackedVector3Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			const PackedVector4Array &arr = v;
			size = arr.size();
			break;
		}
		case Variant::PACKED_COLOR_ARRAY: {
			const PackedColorArray &arr = v;
			size = arr.size();
			break;
		}
		case Variant::DICTIONARY: {
			const Dictionary &dict = v;
			size = dict.size();
			break;
		}
		default:
			return false;
	}
	return true;
}

void RDBuffer::_bind_methods() {
}

RDBuffer::RDBuffer() {
	alignment = 16;
}

RDBuffer::~RDBuffer() {
	if (!is_ref && rid.is_valid()) {
		if (const RenderingServer* rendering_server = RenderingServer::get_singleton()) {
			if (RenderingDevice* rd = rendering_server->get_rendering_device()) {
				rd->free_rid(rid);
			}
		}
	}
}

void RDBuffer::write(const int64_t offset, const int64_t size, const Variant& data) {
	ERR_FAIL_COND_MSG(get_is_fixed_size() && buffer.size() == 0, "Writing fixed-size buffer before initialize!");
	dirty_start = Math::min(offset, dirty_start);
	dirty_end = Math::max(offset + size, dirty_end);
	write(buffer, offset, size, data);
}

void RDBuffer::write_shape(const int64_t offset, const ShaderTypeLayoutShape* shape, const Variant& data) {
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

#define MIN_SIZE(x) \
	if (unlikely(size < x)) return;

void RDBuffer::write(PackedByteArray& destination, const int64_t offset, const int64_t size, const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	ERR_FAIL_COND(offset < 0);
	ERR_FAIL_COND(offset + size > destination.size());
	ERR_FAIL_COND(size < 4);
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
		case Variant::STRING: {
			const String string = data;
			const PackedByteArray array = string.to_utf8_buffer();
			memcpy(destination.ptrw() + offset, array.ptr(), Math::min(array.size(), size));
		}
		case Variant::RECT2: {
			MIN_SIZE(8);
			const Rect2 rect = data;
			write(destination, offset, 8, rect.position);
			MIN_SIZE(16);
			write(destination, offset + 8, 8, rect.size);
			break;
		}
		case Variant::RECT2I: {
			MIN_SIZE(8);
			const Rect2i rect = data;
			write(destination, offset, 8, rect.position);
			MIN_SIZE(16);
			write(destination, offset + 8, 8, rect.size);
			break;
		}
		case Variant::VECTOR2: {
			const Vector2 vector = data;
			destination.encode_float(offset, vector.x);
			MIN_SIZE(8);
			destination.encode_float(offset + 4, vector.y);
			break;
		}
		case Variant::VECTOR3: {
			const Vector3 vector = data;
			destination.encode_float(offset, vector.x);
			MIN_SIZE(8);
			destination.encode_float(offset + 4, vector.y);
			MIN_SIZE(12);
			destination.encode_float(offset + 8, vector.z);
			break;
		}
		case Variant::VECTOR4: {
			const Vector4 vector = data;
			destination.encode_float(offset, vector.x);
			MIN_SIZE(8);
			destination.encode_float(offset + 4, vector.y);
			MIN_SIZE(12);
			destination.encode_float(offset + 8, vector.z);
			MIN_SIZE(16);
			destination.encode_float(offset + 12, vector.w);
			break;
		}
		case Variant::COLOR: {
			const Color color = data;
			destination.encode_float(offset, color.r);
			MIN_SIZE(8);
			destination.encode_float(offset + 4, color.g);
			MIN_SIZE(12);
			destination.encode_float(offset + 8, color.b);
			MIN_SIZE(16);
			destination.encode_float(offset + 12, color.a);
			break;
		}
		case Variant::VECTOR2I: {
			const Vector2i vector = data;
			destination.encode_s32(offset, vector.x);
			MIN_SIZE(8);
			destination.encode_s32(offset + 4, vector.y);
			break;
		}
		case Variant::VECTOR3I: {
			const Vector3i vector = data;
			destination.encode_s32(offset, vector.x);
			MIN_SIZE(8);
			destination.encode_s32(offset + 4, vector.y);
			MIN_SIZE(12);
			destination.encode_s32(offset + 8, vector.z);
			break;
		}
		case Variant::VECTOR4I: {
			const Vector4i vector = data;
			destination.encode_s32(offset, vector.x);
			MIN_SIZE(8);
			destination.encode_s32(offset + 4, vector.y);
			MIN_SIZE(12);
			destination.encode_s32(offset + 8, vector.z);
			MIN_SIZE(16);
			destination.encode_s32(offset + 12, vector.w);
			break;
		}
		case Variant::PLANE: {
			const Plane plane = data;
			destination.encode_float(offset, plane.normal.x);
			MIN_SIZE(8);
			destination.encode_float(offset + 4, plane.normal.y);
			MIN_SIZE(12);
			destination.encode_float(offset + 8, plane.normal.z);
			MIN_SIZE(16);
			destination.encode_float(offset + 12, plane.d);
			break;
		}
		case Variant::QUATERNION: {
			const Quaternion quaternion = data;
			destination.encode_float(offset, quaternion[0]);
			MIN_SIZE(8);
			destination.encode_float(offset + 4, quaternion[1]);
			MIN_SIZE(12);
			destination.encode_float(offset + 8, quaternion[2]);
			MIN_SIZE(16);
			destination.encode_float(offset + 12, quaternion[3]);
			break;
		}
		case Variant::BASIS: {
			MIN_SIZE(16);
			const Basis basis = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(destination, offset, 16, basis[0]);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, basis[1]);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, basis[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector3 col0(basis[0].x, basis[1].x, basis[2].x);
					const Vector3 col1(basis[0].y, basis[1].y, basis[2].y);
					const Vector3 col2(basis[0].z, basis[1].z, basis[2].z);
					write(destination, offset, Math::min<int64_t>(16, size), col0);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, col1);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, col2);
					break;
				}
				default:
					ERR_FAIL_MSG("Invalid matrix layout!");
			}
			break;
		}
		case Variant::PROJECTION: {
			MIN_SIZE(16);
			const Projection projection = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(destination, offset, 16, projection[0]);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, projection[1]);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, projection[2]);
					MIN_SIZE(64);
					write(destination, offset + 48, 16, projection[3]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector4 col0(projection[0].x, projection[1].x, projection[2].x, projection[3].x);
					const Vector4 col1(projection[0].y, projection[1].y, projection[2].y, projection[3].y);
					const Vector4 col2(projection[0].z, projection[1].z, projection[2].z, projection[3].z);
					const Vector4 col3(projection[0].w, projection[1].w, projection[2].w, projection[3].w);
					write(destination, offset, 16, col0);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, col1);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, col2);
					MIN_SIZE(64);
					write(destination, offset + 48, 16, col3);
					break;
				}
				default:
					ERR_FAIL_MSG("Invalid matrix layout!");
			}
			break;
		}
		case Variant::AABB: {
			MIN_SIZE(16);
			const AABB aabb = data;
			write(destination, offset, 16, aabb.position);
			MIN_SIZE(32);
			write(destination, offset + 16, 16, aabb.size);
			break;
		}
		case Variant::TRANSFORM2D: {
			MIN_SIZE(16);
			const Transform2D transform = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(destination, offset, 16, transform[0]);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, transform[1]);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, transform[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector3 col0(transform[0].x, transform[1].x, transform[2].x);
					const Vector3 col1(transform[0].y, transform[1].y, transform[2].y);
					write(destination, offset, 16, col0);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, col1);
					break;
				}
				default:
					ERR_FAIL_MSG("Invalid matrix layout!");
			}
			break;
		}
		case Variant::TRANSFORM3D: {
			MIN_SIZE(16);
			const Transform3D transform = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(destination, offset, 16, transform.basis[0]);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, transform.basis[1]);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, transform.basis[2]);
					MIN_SIZE(64);
					write(destination, offset + 48, 16, transform.origin);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector4 col0(transform.basis[0].x, transform.basis[1].x, transform.basis[2].x, transform.origin.x);
					const Vector4 col1(transform.basis[0].y, transform.basis[1].y, transform.basis[2].y, transform.origin.y);
					const Vector4 col2(transform.basis[0].z, transform.basis[1].z, transform.basis[2].z, transform.origin.z);
					write(destination, offset, 16, col0);
					MIN_SIZE(32);
					write(destination, offset + 16, 16, col1);
					MIN_SIZE(48);
					write(destination, offset + 32, 16, col2);
					break;
				}
				default:
					ERR_FAIL_MSG("Invalid matrix layout!");
			}
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

int64_t RDBuffer::write_shape(PackedByteArray& destination, const int64_t offset, const ShaderTypeLayoutShape* shape, const Variant& data, const bool resize) {
	const auto resource_shape = cast_to<ResourceTypeLayoutShape>(data);
	if (data.get_type() == Variant::PACKED_BYTE_ARRAY || (resource_shape && resource_shape->get_resource_type() == ResourceTypeLayoutShape::RAW_BYTES)) {
		// TODO: Handle other types
		const PackedByteArray bytes = data;
		const int64_t size = bytes.size();

		if (resize && destination.size() < offset + size) {
			destination.resize(offset + size);
		}

		write(destination, offset, size, data);
		return size;
	}
	if (const auto variant_shape = cast_to<VariantTypeLayoutShape>(shape)) {
		const int64_t size = variant_shape->get_size();
		ERR_FAIL_COND_V(size <= 0, 0);

		if (resize && destination.size() < offset + size) {
			destination.resize(offset + size);
		}

		write(destination, offset, size, data, variant_shape->get_matrix_layout());

		return size;
	}
	if (const auto structured_shape = cast_to<StructTypeLayoutShape>(shape)) {
		const Dictionary properties = structured_shape->get_properties();
		const int64_t size = structured_shape->get_size();
		ERR_FAIL_COND_V(size <= 0, 0);

		if (resize && destination.size() < offset + size) {
			destination.resize(offset + size);
		}

		for (const StringName property_name : properties.keys()) {
			const Dictionary property = properties[property_name];

			const int64_t property_offset = property["offset"];
			const Ref<ShaderTypeLayoutShape> property_shape = property["shape"];
			if (property_shape.is_null()) continue;

			const Dictionary property_attributes = property["user_attributes"];

			bool is_valid{};
			Variant property_value = data.get_named(property_name, is_valid);
			if (property_value.get_type() == Variant::Type::NIL && property.has("default_value")) {
				property_value = property["default_value"];
				is_valid = true;
			}

			if (is_valid) {
				// TODO: If this gets any more complex, it needs to move out of here
				if (property_attributes.has(GodotAttributes::color())) {
					if (property_value.get_type() == Variant::COLOR) {
						property_value = static_cast<Color>(property_value).srgb_to_linear();
					}
					if (property_value.get_type() == Variant::PACKED_COLOR_ARRAY) {
						PackedColorArray converted = property_value.duplicate();
						for (Color& color : converted) {
							color = color.srgb_to_linear();
						}
						property_value = converted;
					}
				}

				write_shape(destination, offset + property_offset, property_shape.ptr(), property_value, resize);
			}
		}

		return size;
	}
	if (const auto array_shape = cast_to<ArrayTypeLayoutShape>(shape)) {
		const int64_t stride = array_shape->get_stride();
		ERR_FAIL_COND_V(stride <= 0, 0);

		int64_t size = array_shape->get_size();
		if (size == 0) {
			int64_t container_size;
			if (get_container_size(data, container_size)) {
				size = container_size * stride;
			}
		}

		if (size > 0) {
			if (resize && destination.size() < offset + size) {
				destination.resize(offset + size);
			}
		}

		int64_t element_offset = offset;

		Variant key;
		bool is_valid;
		if (data.iter_init(key, is_valid) && is_valid) {
			const Ref<ShaderTypeLayoutShape> element_shape = array_shape->get_element_shape();
			do {
				Variant value = data.iter_get(key, is_valid);
				if (is_valid) {
					write_shape(destination, element_offset, element_shape.ptr(), value, resize);
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
