#pragma once

#include "godot_cpp/classes/rd_shader_file.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "binding_macros.h"
#include "compute_shader_kernel.h"

using namespace godot;

class SlangShader : public Resource {
	GDCLASS(SlangShader, Resource)

	GET_SET_PROPERTY(TypedArray<ComputeShaderKernel>, kernels)

protected:
	static void _bind_methods();

public:
	SlangShader() = default;
	~SlangShader() override = default;
};
