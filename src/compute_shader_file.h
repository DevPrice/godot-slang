#pragma once

#include "godot_cpp/classes/rd_shader_file.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "compute_shader_kernel.h"

using namespace godot;

class ComputeShaderFile : public Resource {
	GDCLASS(ComputeShaderFile, Resource)

	GET_SET_PROPERTY(TypedArray<ComputeShaderKernel>, kernels)

protected:
	static void _bind_methods();

public:
	ComputeShaderFile() = default;
	~ComputeShaderFile() override = default;
};
