#include "variant_serializer.h"

using namespace godot;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

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
	return std::visit(overloaded{
							  [](const InlineBuffer& arg) {
								  PackedByteArray result{};
								  result.resize(arg.size);
								  memcpy(result.ptrw(), arg.data.data(), arg.size);
								  return result;
							  },
							  [](const PackedByteArray& arg) { return arg; } },
			buffer);
}

VariantSerializer::Buffer::operator std::span<unsigned char>() {
	return std::span(data(), size());
}

VariantSerializer::Buffer::operator std::span<const unsigned char>() const {
	return std::span(data(), size());
}

VariantSerializer::Buffer VariantSerializer::serialize(const Variant& data, const BufferLayout layout, const ShaderTypeLayoutShape::MatrixLayout matrix_layout) {
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
	switch (layout) {
		case BufferLayout::STD140:
			buffer.set_inline_size(Cursor<BufferLayout::STD140>(buffer.data(), Buffer::max_size).write(data, matrix_layout));
			break;
		case BufferLayout::STD430:
			buffer.set_inline_size(Cursor<BufferLayout::STD430>(buffer.data(), Buffer::max_size).write(data, matrix_layout));
			break;
		default:
			ERR_FAIL_V_MSG(buffer, "Invalid buffer layout for serialization!");
	}
	return buffer;
}
