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
	template<BufferLayout Layout>
	class Cursor {
		std::span<uint8_t> destination{};
		size_t offset{};

	public:
		explicit Cursor(const std::span<uint8_t>& p_destination) : destination(p_destination) { }
		explicit Cursor(uint8_t* p_destination, const size_t p_max_size) : destination(p_destination, p_max_size) { }

		template<typename T>
		size_t write(const T& value) {
			static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
			ERR_FAIL_COND_V(destination.size() - offset < sizeof(T), 0);
			memcpy(destination.data() + offset, &value, sizeof(T));
			offset += sizeof(T);
			return sizeof(T);
		}

		size_t write(const godot::Variant& data, const ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR) {
			ERR_FAIL_NULL_V(destination.data(), 0);
			ERR_FAIL_COND_V(destination.empty(), 0);
			const size_t start = offset;
			switch (data.get_type()) {
				case godot::Variant::NIL:
					write<int32_t>(0);
					break;
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
					write<godot::Rect2>(data);
					break;
				}
				case godot::Variant::RECT2I: {
					write<godot::Rect2i>(data);
					break;
				}
				case godot::Variant::VECTOR2: {
					write<godot::Vector2>(data);
					break;
				}
				case godot::Variant::VECTOR3: {
					write<godot::Vector3>(data);
					break;
				}
				case godot::Variant::VECTOR4: {
					write<godot::Vector4>(data);
					break;
				}
				case godot::Variant::COLOR: {
					write<godot::Color>(data);
					break;
				}
				case godot::Variant::VECTOR2I: {
					write<godot::Vector2i>(data);
					break;
				}
				case godot::Variant::VECTOR3I: {
					write<godot::Vector3i>(data);
					break;
				}
				case godot::Variant::VECTOR4I: {
					write<godot::Vector4i>(data);
					break;
				}
				case godot::Variant::PLANE: {
					write<godot::Plane>(data);
					break;
				}
				case godot::Variant::QUATERNION: {
					const godot::Quaternion quaternion = data;
					write(quaternion.components);
					break;
				}
				case godot::Variant::BASIS: {
					const godot::Basis basis = data;
					switch (matrix_layout) {
						case ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR:
							write(basis[0]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(basis[1]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(basis[2]);
							break;
						case ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
							const godot::Vector3 col0(basis[0].x, basis[1].x, basis[2].x);
							const godot::Vector3 col1(basis[0].y, basis[1].y, basis[2].y);
							const godot::Vector3 col2(basis[0].z, basis[1].z, basis[2].z);
							write(col0);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(col1);
							if constexpr (Layout == BufferLayout::STD140) align(16);
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
						case ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR:
							write(projection.columns);
							break;
						case ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
							const std::array rows = {
								godot::Vector4(projection[0].x, projection[1].x, projection[2].x, projection[3].x),
								godot::Vector4(projection[0].y, projection[1].y, projection[2].y, projection[3].y),
								godot::Vector4(projection[0].z, projection[1].z, projection[2].z, projection[3].z),
								godot::Vector4(projection[0].w, projection[1].w, projection[2].w, projection[3].w),
							};
							write(rows);
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
						case ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR:
							write(transform[0]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(transform[1]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(transform[2]);
							break;
						case ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
							const godot::Vector3 col0(transform[0].x, transform[1].x, transform[2].x);
							const godot::Vector3 col1(transform[0].y, transform[1].y, transform[2].y);
							write(col0);
							if constexpr (Layout == BufferLayout::STD140) align(16);
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
						case ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR:
							write(transform.basis[0]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(transform.basis[1]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(transform.basis[2]);
							if constexpr (Layout == BufferLayout::STD140) align(16);
							write(transform.origin);
							break;
						case ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR: {
							const std::array rows = {
								godot::Vector4(transform.basis[0].x, transform.basis[1].x, transform.basis[2].x, transform.origin.x),
								godot::Vector4(transform.basis[0].y, transform.basis[1].y, transform.basis[2].y, transform.origin.y),
								godot::Vector4(transform.basis[0].z, transform.basis[1].z, transform.basis[2].z, transform.origin.z),
							};
							write(rows);
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

		void align(const size_t alignment) {
			const size_t misalignment = offset & alignment;
			if (misalignment > 0) {
				offset += alignment - misalignment;
			}
		}
	};

public:
    static Buffer serialize(const godot::Variant& data, BufferLayout layout, ShaderTypeLayoutShape::MatrixLayout matrix_layout = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR);
};
