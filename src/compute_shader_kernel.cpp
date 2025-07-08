#include "compute_shader_kernel.h"

void ComputeShaderKernel::_bind_methods() {
	BIND_GET_SET(ComputeShaderKernel, kernel_name, Variant::STRING);
	BIND_GET_SET(ComputeShaderKernel, thread_group_size, Variant::VECTOR3);
	BIND_GET_SET(ComputeShaderKernel, user_attributes, Variant::DICTIONARY);
	BIND_GET_SET(ComputeShaderKernel, parameters, Variant::DICTIONARY);
	BIND_GET_SET(ComputeShaderKernel, buffers, Variant::ARRAY);
	BIND_GET_SET_RESOURCE(ComputeShaderKernel, spirv, RDShaderSPIRV);
}

String ComputeShaderKernel::get_compile_error() const {
	const Ref<RDShaderSPIRV> spirv = get_spirv();
	if (spirv.is_valid()) {
		return spirv->get_stage_compile_error(RenderingDevice::SHADER_STAGE_COMPUTE);
	}
	return String();
}

GET_SET_PROPERTY_IMPL(ComputeShaderKernel, StringName, kernel_name);
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Ref<RDShaderSPIRV>, spirv);
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Vector3i, thread_group_size);
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Dictionary, user_attributes);
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Dictionary, parameters);
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, TypedArray<Dictionary>, buffers);
