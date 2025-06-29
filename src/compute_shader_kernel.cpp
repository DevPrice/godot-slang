#include "compute_shader_kernel.h"

void ComputeShaderKernel::_bind_methods() {
	BIND_METHOD(ComputeShaderKernel, get_spirv)
}

GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Ref<RDShaderSPIRV>, spirv)
