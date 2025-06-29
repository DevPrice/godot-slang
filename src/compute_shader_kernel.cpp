#include "compute_shader_kernel.h"

void ComputeShaderKernel::_bind_methods() {
	BIND_GET_SET(ComputeShaderKernel, kernel_name, Variant::STRING, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE)
	BIND_GET_SET_RESOURCE(ComputeShaderKernel, spirv, PROPERTY_USAGE_STORAGE)
}

GET_SET_PROPERTY_IMPL(ComputeShaderKernel, String, kernel_name)
GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Ref<RDShaderSPIRV>, spirv)
