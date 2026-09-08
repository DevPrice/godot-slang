#pragma once

#include "godot_cpp/classes/rd_shader_spirv.hpp"
#include "godot_cpp/classes/resource.hpp"

#include "binding_macros.h"
#include "compute_shader_shape.h"

class SlangShaderProgram : public godot::Resource {
	GDCLASS(SlangShaderProgram, Resource)

	GET_SET_PROPERTY(godot::StringName, program_name)
	GET_SET_PROPERTY(godot::Ref<godot::RDShaderSPIRV>, spirv)
	// Bitmask of RenderingDevice::SHADER_STAGE_*_BIT for the stages this program is built from.
	// Set even when a stage fails to compile, so it says what the program is, not what succeeded.
	GET_SET_PROPERTY(int64_t, stages)
	GET_SET_PROPERTY(godot::Vector3i, thread_group_size)
	GET_SET_PROPERTY(int64_t, space_offset)
	GET_SET_PROPERTY(int64_t, slot_offset)
	GET_SET_PROPERTY(godot::Dictionary, user_attributes)
	GET_SET_PROPERTY(godot::Dictionary, used_binding_sets)
	GET_SET_PROPERTY(godot::Ref<StructTypeLayoutShape>, parameters)

protected:
	static void _bind_methods();

public:
	SlangShaderProgram() = default;
	~SlangShaderProgram() override = default;

	godot::String get_compile_error() const;

	[[nodiscard]] bool has_stage(godot::RenderingDevice::ShaderStage stage) const;

	static int64_t stage_bit(godot::RenderingDevice::ShaderStage stage);
};
