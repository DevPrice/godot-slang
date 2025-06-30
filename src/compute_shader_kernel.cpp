#include "compute_shader_kernel.h"

void ComputeShaderKernel::_bind_methods() {
	BIND_GET_SET(ComputeShaderKernel, kernel_name, Variant::STRING)
	BIND_GET_SET(ComputeShaderKernel, user_attributes, Variant::DICTIONARY)
	BIND_GET_SET(ComputeShaderKernel, parameters, Variant::DICTIONARY)
	BIND_GET_SET_RESOURCE(ComputeShaderKernel, spirv, RDShaderSPIRV)
}

GET_SET_PROPERTY_IMPL(ComputeShaderKernel, String, kernel_name)
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Dictionary, user_attributes)
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Dictionary, parameters)
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Ref<RDShaderSPIRV>, spirv)
