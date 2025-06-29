#include "compute_shader_kernel.h"

void ComputeShaderKernel::_bind_methods() {
	BIND_GET_SET_RESOURCE(ComputeShaderKernel, spirv, PROPERTY_USAGE_STORAGE)
}

GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Ref<RDShaderSPIRV>, spirv)
