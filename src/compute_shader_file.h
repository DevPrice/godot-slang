#pragma once

#include "godot_cpp/classes/rd_shader_file.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "compute_shader_kernel.h"

using namespace godot;

class ComputeShaderFile : public Resource {
	GDCLASS(ComputeShaderFile, Resource)

	GET_SET_PROPERTY(TypedArray<ComputeShaderKernel>, kernels)
	GET_SET_PROPERTY(String, base_error)

protected:
	static void _bind_methods();

public:
	ComputeShaderFile() = default;
	~ComputeShaderFile() override = default;

	void set_bytecode(const Ref<RDShaderSPIRV> &p_bytecode, const StringName &p_version = StringName(), int64_t kernel_index = 0);
	[[nodiscard]] Ref<RDShaderSPIRV> get_spirv(const StringName &p_version = StringName(), int64_t kernel_index = 0) const;
	[[nodiscard]] TypedArray<StringName> get_version_list(int64_t kernel_index = 0) const;

	// Values must match SlangMatrixLayoutMode
	enum MatrixLayout {
		UNKNOWN = 0,
		ROW_MAJOR = 1,
		COLUMN_MAJOR = 2,
	};
};

VARIANT_ENUM_CAST(ComputeShaderFile::MatrixLayout)
