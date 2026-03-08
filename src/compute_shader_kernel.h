#pragma once

#include "godot_cpp/classes/rd_shader_spirv.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "compute_shader_shape.h"

class ComputeShaderKernel : public godot::Resource {
	GDCLASS(ComputeShaderKernel, Resource)

	GET_SET_PROPERTY(godot::StringName, kernel_name)
	GET_SET_PROPERTY(godot::Ref<godot::RDShaderSPIRV>, spirv)
	GET_SET_PROPERTY(godot::Vector3i, thread_group_size)
	GET_SET_PROPERTY(int64_t, space_offset)
	GET_SET_PROPERTY(int64_t, slot_offset)
	GET_SET_PROPERTY(godot::Dictionary, user_attributes)
	GET_SET_PROPERTY(godot::Dictionary, used_binding_sets)
	GET_SET_PROPERTY(godot::Ref<StructTypeLayoutShape>, parameters)

protected:
	static void _bind_methods();

public:
	ComputeShaderKernel() = default;
	~ComputeShaderKernel() override = default;

	godot::String get_compile_error() const;
};
