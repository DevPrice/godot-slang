#include "compute_shader_kernel.h"

void ComputeShaderKernel::_bind_methods() {
	BIND_GET_SET_RESOURCE(ComputeShaderKernel, shader_file, RDShaderFile)
	BIND_METHOD(ComputeShaderKernel, get_spirv)
}

Ref<RDShaderSPIRV> ComputeShaderKernel::get_spirv() const {
	if (shader_file.is_null()) {
		return nullptr;
	}
	return shader_file->get_spirv();
}

GET_SET_PROPERTY_IMPL(ComputeShaderKernel, Ref<RDShaderFile>, shader_file)
