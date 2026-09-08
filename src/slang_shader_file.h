#pragma once

#include "godot_cpp/classes/rd_shader_file.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "slang_shader_program.h"
#include "compute_shader_shape.h"

class SlangShaderFile : public godot::Resource {
	GDCLASS(SlangShaderFile, Resource)

	GET_SET_PROPERTY(godot::TypedArray<SlangShaderProgram>, kernels)
	GET_SET_PROPERTY(godot::TypedArray<SlangShaderProgram>, passes)
	GET_SET_PROPERTY(godot::String, base_error)
	GET_SET_PROPERTY(godot::Ref<StructTypeLayoutShape>, parameters)

protected:
	static void _bind_methods();

public:
	SlangShaderFile() = default;
	~SlangShaderFile() override = default;

	void set_bytecode(const godot::Ref<godot::RDShaderSPIRV> &p_bytecode, const godot::StringName &p_version = godot::StringName(), int64_t kernel_index = 0);
	[[nodiscard]] godot::Ref<godot::RDShaderSPIRV> get_spirv(const godot::StringName &p_version = godot::StringName(), int64_t kernel_index = 0) const;
	[[nodiscard]] godot::TypedArray<godot::StringName> get_version_list(int64_t kernel_index = 0) const;

	[[nodiscard]] godot::Ref<godot::RDShaderSPIRV> get_pass_spirv(const godot::StringName &p_version = godot::StringName(), int64_t pass_index = 0) const;

	enum MatrixLayout {
		UNKNOWN = ShaderTypeLayoutShape::MatrixLayout::UNKNOWN,
		ROW_MAJOR = ShaderTypeLayoutShape::MatrixLayout::ROW_MAJOR,
		COLUMN_MAJOR = ShaderTypeLayoutShape::MatrixLayout::COLUMN_MAJOR,
	};

	static godot::String get_godot_version_string();
};

VARIANT_ENUM_CAST(SlangShaderFile::MatrixLayout)
