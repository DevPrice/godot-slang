#include "variant_serializer.h"

void VariantSerializer::align(const size_t alignment) {
	const size_t misalignment = offset & alignment;
	if (misalignment > 0) {
		offset += alignment - misalignment;
	}
}

size_t VariantSerializer::serialize(const Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
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
			serialize(rect.position, matrix_layout);
			serialize(rect.size, matrix_layout);
			break;
		}
		case Variant::RECT2I: {
			const Rect2i rect = data;
			serialize(rect.position, matrix_layout);
			serialize(rect.size, matrix_layout);
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
					serialize(basis[0]);
					align(16);
					serialize(basis[1]);
					align(16);
					serialize(basis[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector3 col0(basis[0].x, basis[1].x, basis[2].x);
					const Vector3 col1(basis[0].y, basis[1].y, basis[2].y);
					const Vector3 col2(basis[0].z, basis[1].z, basis[2].z);
					serialize(col0);
					align(16);
					serialize(col1);
					align(16);
					serialize(col2);
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
					serialize(projection[0]);
					serialize(projection[1]);
					serialize(projection[2]);
					serialize(projection[3]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector4 col0(projection[0].x, projection[1].x, projection[2].x, projection[3].x);
					const Vector4 col1(projection[0].y, projection[1].y, projection[2].y, projection[3].y);
					const Vector4 col2(projection[0].z, projection[1].z, projection[2].z, projection[3].z);
					const Vector4 col3(projection[0].w, projection[1].w, projection[2].w, projection[3].w);
					serialize(col0);
					serialize(col1);
					serialize(col2);
					serialize(col3);
					break;
				}
				default:
					ERR_FAIL_V_MSG(offset - start, "Invalid matrix layout!");
			}
			break;
		}
		case Variant::AABB: {
			const AABB aabb = data;
			serialize(aabb.position);
			align(16);
			serialize(aabb.size);
			break;
		}
		case Variant::TRANSFORM2D: {
			const Transform2D transform = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					serialize(transform[0]);
					align(16);
					serialize(transform[1]);
					align(16);
					serialize(transform[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector3 col0(transform[0].x, transform[1].x, transform[2].x);
					const Vector3 col1(transform[0].y, transform[1].y, transform[2].y);
					serialize(col0);
					align(16);
					serialize(col1);
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
					serialize(transform.basis[0]);
					align(16);
					serialize(transform.basis[1]);
					align(16);
					serialize(transform.basis[2]);
					align(16);
					serialize(transform.origin);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const Vector4 col0(transform.basis[0].x, transform.basis[1].x, transform.basis[2].x, transform.origin.x);
					const Vector4 col1(transform.basis[0].y, transform.basis[1].y, transform.basis[2].y, transform.origin.y);
					const Vector4 col2(transform.basis[0].z, transform.basis[1].z, transform.basis[2].z, transform.origin.z);
					serialize(col0);
					serialize(col1);
					serialize(col2);
					break;
				}
				default:
					ERR_FAIL_V_MSG(offset - start, "Invalid matrix layout!");
			}
			break;
		}
		case Variant::STRING: {
			// TODO: Handle large strings that would overflow the max_size
			const String string = data;
			const PackedByteArray array = string.to_utf8_buffer();
			const size_t size = Math::min(static_cast<size_t>(array.size()), max_size);
			memcpy(destination, array.ptr(), size);
			offset += size;
			break;
		}
		case Variant::PACKED_BYTE_ARRAY: {
			// TODO: Handle large byte arrays that would overflow the max_size
			const PackedByteArray& array = data;
			const size_t size = Math::min(static_cast<size_t>(array.size()), max_size);
			memcpy(destination, array.ptr(), size);
			offset += size;
			break;
		}
		default:
			// TODO: Handle other Packed*Array types
			ERR_FAIL_V_MSG(offset - start, String("Unsupported data type: ") + Variant::get_type_name(data.get_type()));
	}
	return offset - start;
}
