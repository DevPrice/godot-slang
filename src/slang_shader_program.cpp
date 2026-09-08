#include "slang_shader_program.h"

using namespace godot;

void SlangShaderProgram::_bind_methods() {
	BIND_GET_SET(SlangShaderProgram, kernel_name, Variant::STRING);
	BIND_GET_SET(SlangShaderProgram, thread_group_size, Variant::VECTOR3);
	BIND_GET_SET(SlangShaderProgram, user_attributes, Variant::DICTIONARY);
	BIND_GET_SET(SlangShaderProgram, used_binding_sets, Variant::DICTIONARY);
	BIND_GET_SET(SlangShaderProgram, space_offset, Variant::INT);
	BIND_GET_SET(SlangShaderProgram, slot_offset, Variant::INT);
	BIND_GET_SET_RESOURCE(SlangShaderProgram, spirv, RDShaderSPIRV);
	BIND_GET_SET(SlangShaderProgram, stages, Variant::INT);
	BIND_GET_SET_RESOURCE(SlangShaderProgram, parameters, StructTypeLayoutShape);
	BIND_METHOD(SlangShaderProgram, get_compile_error)
}

String SlangShaderProgram::get_compile_error() const {
	const Ref<RDShaderSPIRV> spirv = get_spirv();
	if (spirv.is_null()) {
		return {};
	}
	// a program can span several stages, and any one of them can be the one that failed
	for (int32_t stage = 0; stage < RenderingDevice::SHADER_STAGE_MAX; ++stage) {
		const String stage_error = spirv->get_stage_compile_error(static_cast<RenderingDevice::ShaderStage>(stage));
		if (!stage_error.is_empty()) {
			return stage_error;
		}
	}
	return {};
}

int64_t SlangShaderProgram::stage_bit(const RenderingDevice::ShaderStage stage) {
	return int64_t(1) << static_cast<int64_t>(stage);
}

GET_SET_PROPERTY_IMPL(SlangShaderProgram, StringName, kernel_name);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, Ref<RDShaderSPIRV>, spirv);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, int64_t, stages);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, Vector3i, thread_group_size);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, int64_t, space_offset);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, int64_t, slot_offset);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, Dictionary, user_attributes);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, Dictionary, used_binding_sets);
GET_SET_PROPERTY_IMPL(SlangShaderProgram, Ref<StructTypeLayoutShape>, parameters);
