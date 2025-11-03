#pragma once

#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "compute_shader_file.h"

using namespace godot;

class ComputeShaderShape : public Resource {
	GDCLASS(ComputeShaderShape, Resource);

protected:
	static void _bind_methods();

public:
	[[nodiscard]] virtual int64_t get_size() const { return 0; };

};

class ComputeShaderVariantShape : public ComputeShaderShape {
	GDCLASS(ComputeShaderVariantShape, ComputeShaderShape);

protected:
	static void _bind_methods();

	GET_SET_PROPERTY(ComputeShaderFile::MatrixLayout, matrix_layout)

public:
	[[nodiscard]] int64_t get_size() const override;
	void set_size(int64_t p_size);

private:
	int64_t size{};

};

class ComputeShaderArrayShape : public ComputeShaderShape {
	GDCLASS(ComputeShaderArrayShape, ComputeShaderShape);

protected:
	static void _bind_methods();

	GET_SET_PROPERTY(Ref<ComputeShaderShape>, element_shape)
	GET_SET_PROPERTY(int64_t, stride)
	GET_SET_PROPERTY(int64_t, alignment)

public:
	[[nodiscard]] int64_t get_size() const override;
	void set_size(int64_t p_size);

private:
	int64_t size{};

};

class ComputeShaderStructuredShape : public ComputeShaderShape {
	GDCLASS(ComputeShaderStructuredShape, ComputeShaderShape);

protected:
	static void _bind_methods();

	GET_SET_PROPERTY(int64_t, alignment)
	GET_SET_PROPERTY(Dictionary, properties)
	GET_SET_PROPERTY(Dictionary, user_attributes)

public:
	[[nodiscard]] int64_t get_size() const override;
	void set_size(int64_t p_size);

private:
	int64_t size{};

};

class ComputeShaderResourceShape : public ComputeShaderShape {
	GDCLASS(ComputeShaderResourceShape, ComputeShaderShape);

public:
	enum ComputeShaderResourceType {
		UNKNOWN,
		RAW_BYTES,
	};

	GET_SET_PROPERTY(ComputeShaderResourceType, resource_type)

protected:
	static void _bind_methods();

};

VARIANT_ENUM_CAST(ComputeShaderResourceShape::ComputeShaderResourceType)
