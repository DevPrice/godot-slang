#pragma once

#include "godot_cpp/classes/rd_shader_spirv.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"

using namespace godot;

class ComputeShaderKernel : public Resource {
	GDCLASS(ComputeShaderKernel, Resource);

	GET_SET_PROPERTY(Ref<RDShaderSPIRV>, spirv)

protected:
	static void _bind_methods();

public:
	ComputeShaderKernel() = default;
	~ComputeShaderKernel() override = default;
};
