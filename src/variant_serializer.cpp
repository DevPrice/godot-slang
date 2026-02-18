#include "variant_serializer.h"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

VariantSerializer::Buffer::Buffer() : buffer(InlineBuffer{}) { }

VariantSerializer::Buffer::Buffer(const PackedByteArray& p_array) : buffer(p_array) { }

void VariantSerializer::Buffer::set_inline_size(size_t size) {
	return std::visit(overloaded {
		[size](InlineBuffer& arg) { arg.size = size; },
		[](PackedByteArray&) { }
	}, buffer);
}

uint8_t* VariantSerializer::Buffer::data() {
	return std::visit(overloaded {
		[](InlineBuffer& arg) { return arg.data.data(); },
		[](PackedByteArray& arg) { return arg.ptrw(); }
	}, buffer);
}

const uint8_t* VariantSerializer::Buffer::data() const {
	return std::visit(overloaded {
		[](const InlineBuffer& arg) { return arg.data.data(); },
		[](const PackedByteArray& arg) { return arg.ptr(); }
	}, buffer);
}

size_t VariantSerializer::Buffer::size() const {
	return std::visit(overloaded {
		[](const InlineBuffer& arg) { return arg.size; },
		[](const PackedByteArray& arg) { return static_cast<size_t>(arg.size()); }
	}, buffer);
}

void VariantSerializer::Buffer::copy(uint8_t* destination, const size_t max_size) const {
	memcpy(destination, data(), Math::min(size(), max_size));
}

int64_t VariantSerializer::Buffer::compare(const uint8_t* other, const size_t max_size) const {
	return memcmp(this, other, Math::min(size(), max_size));
}

PackedByteArray VariantSerializer::Buffer::as_packed_byte_array() const {
	return std::visit(overloaded {
		[](const InlineBuffer& arg) {
			PackedByteArray result{};
			result.resize(arg.size);
			memcpy(result.ptrw(), arg.data.data(), arg.size);
			return result;
		},
		[](const PackedByteArray& arg) { return arg; }
	}, buffer);
}

void VariantSerializer::align(const size_t alignment) {
	const size_t misalignment = offset & alignment;
	if (misalignment > 0) {
		offset += alignment - misalignment;
	}
}

size_t VariantSerializer::write(const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	ERR_FAIL_NULL_V(destination, 0);
	ERR_FAIL_COND_V(max_size < 4, 0);
	size_t start = offset;
	switch (data.get_type()) {
		case Variant::NIL:
			write<int32_t>(0);
		case Variant::BOOL:
			write<int32_t>(data ? 1 : 0);
			break;
		case Variant::INT:
			write<int32_t>(data);
			break;
		case Variant::FLOAT:
			write<real_t>(data);
			break;
		case Variant::RECT2: {
			const Rect2 rect = data;
			write(rect.position, matrix_layout);
			write(rect.size, matrix_layout);
			break;
		}
		case Variant::RECT2I: {
			const Rect2i rect = data;
			write(rect.position, matrix_layout);
			write(rect.size, matrix_layout);
			break;
		}
		case Variant::VECTOR2: {
			const Vector2 vector = data;
			write<real_t>(vector.x);
			write<real_t>(vector.y);
			break;
		}
		case Variant::VECTOR3: {
			const Vector3 vector = data;
			write<real_t>(vector.x);
			write<real_t>(vector.y);
			write<real_t>(vector.z);
			break;
		}
		case Variant::VECTOR4: {
			const Vector4 vector = data;
			write<real_t>(vector.x);
			write<real_t>(vector.y);
			write<real_t>(vector.z);
			write<real_t>(vector.w);
			break;
		}
		case Variant::COLOR: {
			const Color color = data;
			write<real_t>(color.r);
			write<real_t>(color.g);
			write<real_t>(color.b);
			write<real_t>(color.a);
			break;
		}
		case Variant::VECTOR2I: {
			const Vector2i vector = data;
			write<int32_t>(vector.x);
			write<int32_t>(vector.y);
			break;
		}
		case Variant::VECTOR3I: {
			const Vector3i vector = data;
			write<int32_t>(vector.x);
			write<int32_t>(vector.y);
			write<int32_t>(vector.z);
			break;
		}
		case Variant::VECTOR4I: {
			const Vector4i vector = data;
			write<int32_t>(vector.x);
			write<int32_t>(vector.y);
			write<int32_t>(vector.z);
			write<int32_t>(vector.w);
			break;
		}
		case Variant::PLANE: {
			const Plane plane = data;
			write<real_t>(plane.normal.x);
			write<real_t>(plane.normal.y);
			write<real_t>(plane.normal.z);
			write<real_t>(plane.d);
			break;
		}
		case Variant::QUATERNION: {
			const Quaternion quaternion = data;
			write<real_t>(quaternion[0]);
			write<real_t>(quaternion[1]);
			write<real_t>(quaternion[2]);
			write<real_t>(quaternion[3]);
			break;
		}
		case Variant::BASIS: {
			const Basis basis = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(basis[0]);
					align(16);
					write(basis[1]);
					align(16);
					write(basis[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector3 col0(basis[0].x, basis[1].x, basis[2].x);
					const Vector3 col1(basis[0].y, basis[1].y, basis[2].y);
					const Vector3 col2(basis[0].z, basis[1].z, basis[2].z);
					write(col0);
					align(16);
					write(col1);
					align(16);
					write(col2);
					break;
				}
				default:
					ERR_FAIL_V_MSG(offset - start, "Invalid matrix layout!");
			}
			break;
		}
		case Variant::PROJECTION: {
			const Projection projection = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(projection[0]);
					write(projection[1]);
					write(projection[2]);
					write(projection[3]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector4 col0(projection[0].x, projection[1].x, projection[2].x, projection[3].x);
					const Vector4 col1(projection[0].y, projection[1].y, projection[2].y, projection[3].y);
					const Vector4 col2(projection[0].z, projection[1].z, projection[2].z, projection[3].z);
					const Vector4 col3(projection[0].w, projection[1].w, projection[2].w, projection[3].w);
					write(col0);
					write(col1);
					write(col2);
					write(col3);
					break;
				}
				default:
					ERR_FAIL_V_MSG(offset - start, "Invalid matrix layout!");
			}
			break;
		}
		case Variant::AABB: {
			const AABB aabb = data;
			write(aabb.position);
			align(16);
			write(aabb.size);
			break;
		}
		case Variant::TRANSFORM2D: {
			const Transform2D transform = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(transform[0]);
					align(16);
					write(transform[1]);
					align(16);
					write(transform[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector3 col0(transform[0].x, transform[1].x, transform[2].x);
					const Vector3 col1(transform[0].y, transform[1].y, transform[2].y);
					write(col0);
					align(16);
					write(col1);
					break;
				}
				default:
					ERR_FAIL_V_MSG(offset - start, "Invalid matrix layout!");
			}
			break;
		}
		case Variant::TRANSFORM3D: {
			const Transform3D transform = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(transform.basis[0]);
					align(16);
					write(transform.basis[1]);
					align(16);
					write(transform.basis[2]);
					align(16);
					write(transform.origin);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector4 col0(transform.basis[0].x, transform.basis[1].x, transform.basis[2].x, transform.origin.x);
					const Vector4 col1(transform.basis[0].y, transform.basis[1].y, transform.basis[2].y, transform.origin.y);
					const Vector4 col2(transform.basis[0].z, transform.basis[1].z, transform.basis[2].z, transform.origin.z);
					write(col0);
					write(col1);
					write(col2);
					break;
				}
				default:
					ERR_FAIL_V_MSG(offset - start, "Invalid matrix layout!");
			}
			break;
		}
		default:
			ERR_FAIL_V_MSG(offset - start, String("Unsupported data type for serialization: ") + Variant::get_type_name(data.get_type()));
	}
	return offset - start;
}

VariantSerializer::Buffer VariantSerializer::serialize(const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	switch (data.get_type()) {
		case Variant::STRING:
		case Variant::NODE_PATH: {
			const String& string = data;
			return string.to_utf8_buffer();
		}
		case Variant::STRING_NAME: {
			const StringName& string = data;
			return string.to_utf8_buffer();
		}
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray& array = data;
			return array;
		}
		case Variant::PACKED_INT32_ARRAY: {
			const PackedInt32Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_INT64_ARRAY: {
			const PackedInt64Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_STRING_ARRAY: {
			const PackedStringArray& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			const PackedVector2Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			const PackedVector3Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			const PackedVector4Array& array = data;
			return array.to_byte_array();
		}
		case Variant::PACKED_COLOR_ARRAY: {
			const PackedColorArray& array = data;
			return array.to_byte_array();
		}
		default:
			break;
	}
	Buffer buffer{};
	const size_t size = VariantSerializer(buffer.data(), Buffer::max_size).write(data, matrix_layout);
	buffer.set_inline_size(size);
	return buffer;
}
