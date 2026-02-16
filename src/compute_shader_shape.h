#pragma once

#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "godot_cpp/classes/rendering_device.hpp"

class ComputeShaderCursor;

using namespace godot;

class ShaderTypeLayoutShape : public Resource {
	GDCLASS(ShaderTypeLayoutShape, Resource);

	GET_SET_PROPERTY(TypedArray<Dictionary>, bindings)

protected:
	static void _bind_methods();

public:
	[[nodiscard]] virtual int64_t get_size() const { return 0; }
	virtual void write_into(const ComputeShaderCursor& cursor, const Variant& data) const = 0;

	// Values must match SlangMatrixLayoutMode
	enum MatrixLayout {
		UNKNOWN = 0,
		ROW_MAJOR = 1,
		COLUMN_MAJOR = 2,
	};

	// Values must match SlangParameterCategory
	enum LayoutUnit {
		NONE = 0,
		UNIFORM = 8,
		DESCRIPTOR_TABLE_SLOT = 9,
		PUSH_CONSTANT_BUFFER = 11,
	};
};

class VariantTypeLayoutShape : public ShaderTypeLayoutShape {
	GDCLASS(VariantTypeLayoutShape, ShaderTypeLayoutShape);

protected:
	static void _bind_methods();

	GET_SET_PROPERTY(MatrixLayout, matrix_layout)

public:
	[[nodiscard]] int64_t get_size() const override;
	void set_size(int64_t p_size);
	void write_into(const ComputeShaderCursor& cursor, const Variant& data) const override;

private:
	int64_t size{};

};

class ArrayTypeLayoutShape : public ShaderTypeLayoutShape {
	GDCLASS(ArrayTypeLayoutShape, ShaderTypeLayoutShape);

protected:
	static void _bind_methods();

	GET_SET_PROPERTY(Ref<ShaderTypeLayoutShape>, element_shape)
	GET_SET_PROPERTY(int64_t, stride)
	GET_SET_PROPERTY(int64_t, alignment)

public:
	[[nodiscard]] int64_t get_size() const override;
	void set_size(int64_t p_size);
	void write_into(const ComputeShaderCursor& cursor, const Variant& data) const override;

private:
	int64_t size{};

};

class StructPropertyShape : public Resource {
	GDCLASS(StructPropertyShape, Resource);

protected:
	static void _bind_methods();
};

class StructTypeLayoutShape : public ShaderTypeLayoutShape {
	GDCLASS(StructTypeLayoutShape, ShaderTypeLayoutShape);

protected:
	static void _bind_methods();

	GET_SET_PROPERTY(int64_t, alignment)
	GET_SET_PROPERTY(Dictionary, properties)
	GET_SET_PROPERTY(Dictionary, user_attributes)

public:
	[[nodiscard]] int64_t get_size() const override;
	void set_size(int64_t p_size);
	void write_into(const ComputeShaderCursor& cursor, const Variant& data) const override;

private:
	int64_t size{};

};

class ResourceTypeLayoutShape : public ShaderTypeLayoutShape {
	GDCLASS(ResourceTypeLayoutShape, ShaderTypeLayoutShape);

public:
	enum ComputeShaderResourceType {
		UNKNOWN,
		RAW_BYTES,
	};

	GET_SET_PROPERTY(ComputeShaderResourceType, resource_type)
	GET_SET_PROPERTY(RenderingDevice::UniformType, uniform_type)

public:
	void write_into(const ComputeShaderCursor& cursor, const Variant& data) const override;

protected:
	static void _bind_methods();

};

VARIANT_ENUM_CAST(ShaderTypeLayoutShape::MatrixLayout)
VARIANT_ENUM_CAST(ResourceTypeLayoutShape::ComputeShaderResourceType)
