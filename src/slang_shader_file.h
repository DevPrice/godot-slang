#pragma once

#include "godot_cpp/classes/rd_shader_file.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/classes/shader.hpp"

#include "binding_macros.h"
#include "slang_shader_program.h"
#include "compute_shader_shape.h"

class SlangShaderFile : public godot::Resource {
	GDCLASS(SlangShaderFile, Resource)

	GET_SET_PROPERTY(godot::TypedArray<SlangShaderProgram>, programs)
	GET_SET_PROPERTY(godot::String, base_error)
	GET_SET_PROPERTY(godot::Ref<StructTypeLayoutShape>, parameters)
	// The material declared by this file's [gd::Material] entry point, if it has one. Null for
	// a file that only declares compute kernels or raster passes.
	GET_SET_PROPERTY(godot::Ref<godot::Shader>, material_shader)
	// The generated GDShader source behind `material_shader`. Kept so it can be read back when
	// Godot reports an error against generated code the shader's author never wrote.
	GET_SET_PROPERTY(godot::String, material_source)
	GET_SET_PROPERTY(godot::String, material_error)

protected:
	static void _bind_methods();

public:
	SlangShaderFile() = default;
	~SlangShaderFile() override = default;

	void set_bytecode(const godot::Ref<godot::RDShaderSPIRV> &p_bytecode, const godot::StringName &p_version = godot::StringName(), int64_t program_index = 0);
	[[nodiscard]] godot::Ref<godot::RDShaderSPIRV> get_spirv(const godot::StringName &p_version = godot::StringName(), int64_t program_index = 0) const;
	[[nodiscard]] godot::TypedArray<godot::StringName> get_version_list(int64_t program_index = 0) const;

	enum MatrixLayout {
		UNKNOWN = ShaderTypeLayoutShape::MatrixLayout::UNKNOWN,
		ROW_MAJOR = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR,
		COLUMN_MAJOR = ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR,
	};

	static godot::String get_godot_version_string();
};

VARIANT_ENUM_CAST(SlangShaderFile::MatrixLayout)
