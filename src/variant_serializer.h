#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <variant>

#include "compute_shader_shape.h"
#include "godot_cpp/core/error_macros.hpp"

enum class BufferLayout {
	STD140,
	STD430,
};

class VariantSerializer {

public:
    class Buffer {
    private:
        static constexpr size_t max_size = 64;

        struct InlineBuffer {
            std::array<uint8_t, max_size> data{};
            size_t size = 0;
            InlineBuffer() = default;
        };

        std::variant<InlineBuffer, godot::PackedByteArray> buffer;

        Buffer();
        Buffer(const godot::PackedByteArray& p_array);

        void set_inline_size(size_t size);

        friend class VariantSerializer;

    public:
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        Buffer(Buffer&&) noexcept = default;
        Buffer& operator=(Buffer&&) noexcept = default;

        [[nodiscard]] uint8_t* data();
        [[nodiscard]] const uint8_t* data() const;
        [[nodiscard]] size_t size() const;

        void copy(uint8_t* destination, size_t max_size) const;
        int64_t compare(const uint8_t* other, size_t max_size) const;
    	godot::PackedByteArray as_packed_byte_array() const;
    	operator std::span<uint8_t>();
    	operator std::span<const uint8_t>() const;
	};

private:
	class Cursor {
		std::span<uint8_t> destination{};
		size_t offset{};

	public:
		explicit Cursor(const std::span<uint8_t>& p_destination) : destination(p_destination) { }
		explicit Cursor(uint8_t* p_destination, const size_t p_max_size) : destination(p_destination, p_max_size) { }

		template<typename T>
		size_t write(T value) {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
			ERR_FAIL_COND_V(destination.size() - offset < sizeof(T), 0);
			memcpy(destination.data() + offset, &value, sizeof(T));
			offset += sizeof(T);
			return sizeof(T);
		}

		size_t write(const godot::Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);

		void align(const size_t alignment) {
			const size_t misalignment = offset & alignment;
			if (misalignment > 0) {
				offset += alignment - misalignment;
			}
		}
	};


public:
    static Buffer serialize(const godot::Variant& data, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
};

inline size_t VariantSerializer::Cursor::write(const godot::Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
	ERR_FAIL_NULL_V(destination.data(), 0);
	ERR_FAIL_COND_V(destination.empty(), 0);
	size_t start = offset;
	switch (data.get_type()) {
		case godot::Variant::NIL:
			write<int32_t>(0);
		case godot::Variant::BOOL:
			write<int32_t>(data ? 1 : 0);
			break;
		case godot::Variant::INT:
			write<int32_t>(data);
			break;
		case godot::Variant::FLOAT:
			write<real_t>(data);
			break;
		case godot::Variant::RECT2: {
			const godot::Rect2 rect = data;
			write(rect.position, matrix_layout);
			write(rect.size, matrix_layout);
			break;
		}
		case godot::Variant::RECT2I: {
			const godot::Rect2i rect = data;
			write(rect.position, matrix_layout);
			write(rect.size, matrix_layout);
			break;
		}
		case godot::Variant::VECTOR2: {
			const godot::Vector2 vector = data;
			write<real_t>(vector.x);
			write<real_t>(vector.y);
			break;
		}
		case godot::Variant::VECTOR3: {
			const godot::Vector3 vector = data;
			write<real_t>(vector.x);
			write<real_t>(vector.y);
			write<real_t>(vector.z);
			break;
		}
		case godot::Variant::VECTOR4: {
			const godot::Vector4 vector = data;
			write<real_t>(vector.x);
			write<real_t>(vector.y);
			write<real_t>(vector.z);
			write<real_t>(vector.w);
			break;
		}
		case godot::Variant::COLOR: {
			const godot::Color color = data;
			write<real_t>(color.r);
			write<real_t>(color.g);
			write<real_t>(color.b);
			write<real_t>(color.a);
			break;
		}
		case godot::Variant::VECTOR2I: {
			const godot::Vector2i vector = data;
			write<int32_t>(vector.x);
			write<int32_t>(vector.y);
			break;
		}
		case godot::Variant::VECTOR3I: {
			const godot::Vector3i vector = data;
			write<int32_t>(vector.x);
			write<int32_t>(vector.y);
			write<int32_t>(vector.z);
			break;
		}
		case godot::Variant::VECTOR4I: {
			const godot::Vector4i vector = data;
			write<int32_t>(vector.x);
			write<int32_t>(vector.y);
			write<int32_t>(vector.z);
			write<int32_t>(vector.w);
			break;
		}
		case godot::Variant::PLANE: {
			const godot::Plane plane = data;
			write<real_t>(plane.normal.x);
			write<real_t>(plane.normal.y);
			write<real_t>(plane.normal.z);
			write<real_t>(plane.d);
			break;
		}
		case godot::Variant::QUATERNION: {
			const godot::Quaternion quaternion = data;
			write<real_t>(quaternion[0]);
			write<real_t>(quaternion[1]);
			write<real_t>(quaternion[2]);
			write<real_t>(quaternion[3]);
			break;
		}
		case godot::Variant::BASIS: {
			const godot::Basis basis = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(basis[0]);
					align(16);
					write(basis[1]);
					align(16);
					write(basis[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const godot::Vector3 col0(basis[0].x, basis[1].x, basis[2].x);
					const godot::Vector3 col1(basis[0].y, basis[1].y, basis[2].y);
					const godot::Vector3 col2(basis[0].z, basis[1].z, basis[2].z);
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
		case godot::Variant::PROJECTION: {
			const godot::Projection projection = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(projection[0]);
					write(projection[1]);
					write(projection[2]);
					write(projection[3]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const godot::Vector4 col0(projection[0].x, projection[1].x, projection[2].x, projection[3].x);
					const godot::Vector4 col1(projection[0].y, projection[1].y, projection[2].y, projection[3].y);
					const godot::Vector4 col2(projection[0].z, projection[1].z, projection[2].z, projection[3].z);
					const godot::Vector4 col3(projection[0].w, projection[1].w, projection[2].w, projection[3].w);
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
		case godot::Variant::AABB: {
			const godot::AABB aabb = data;
			write(aabb.position);
			align(16);
			write(aabb.size);
			break;
		}
		case godot::Variant::TRANSFORM2D: {
			const godot::Transform2D transform = data;
			switch (matrix_layout) {
				case StructTypeLayoutShape::MatrixLayout::ROW_MAJOR:
					write(transform[0]);
					align(16);
					write(transform[1]);
					align(16);
					write(transform[2]);
					break;
				case StructTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
					const godot::Vector3 col0(transform[0].x, transform[1].x, transform[2].x);
					const godot::Vector3 col1(transform[0].y, transform[1].y, transform[2].y);
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
		case godot::Variant::TRANSFORM3D: {
			const godot::Transform3D transform = data;
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
					const godot::Vector4 col0(transform.basis[0].x, transform.basis[1].x, transform.basis[2].x, transform.origin.x);
					const godot::Vector4 col1(transform.basis[0].y, transform.basis[1].y, transform.basis[2].y, transform.origin.y);
					const godot::Vector4 col2(transform.basis[0].z, transform.basis[1].z, transform.basis[2].z, transform.origin.z);
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
			ERR_FAIL_V_MSG(offset - start, godot::String("Unsupported data type for serialization: ") + godot::Variant::get_type_name(data.get_type()));
	}
	return offset - start;
}
