#pragma once

#include "godot_cpp/classes/rd_shader_spirv.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"

using namespace godot;

class ComputeShaderKernel : public Resource {
	GDCLASS(ComputeShaderKernel, Resource)

	GET_SET_PROPERTY(StringName, kernel_name)
	GET_SET_PROPERTY(Ref<RDShaderSPIRV>, spirv)
	GET_SET_PROPERTY(Vector3i, thread_group_size)
	GET_SET_PROPERTY(Dictionary, user_attributes)
	GET_SET_PROPERTY(Dictionary, parameters)
	GET_SET_PROPERTY(TypedArray<Dictionary>, buffers)

protected:
	static void _bind_methods();

public:
	ComputeShaderKernel() = default;
	~ComputeShaderKernel() override = default;
};
