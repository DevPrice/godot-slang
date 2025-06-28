#pragma once

#include "godot_cpp/classes/rd_shader_file.hpp"
#include "godot_cpp/classes/rd_shader_spirv.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/variant.hpp"

#include "binding_macros.h"

using namespace godot;

class ComputeShaderKernel : public Resource {
	GDCLASS(ComputeShaderKernel, Resource);

	GET_SET_PROPERTY(Ref<godot::RDShaderFile>, shader_file)

protected:
	static void _bind_methods();

public:
	ComputeShaderKernel() = default;
	~ComputeShaderKernel() override = default;

	Ref<RDShaderSPIRV> get_spirv() const;
};
