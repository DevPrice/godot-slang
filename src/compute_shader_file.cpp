#include "compute_shader_file.h"

void ComputeShaderFile::_bind_methods(){
	BIND_GET_SET_RESOURCE_ARRAY(ComputeShaderFile, kernels, ComputeShaderKernel)
}

GET_SET_PROPERTY_IMPL(ComputeShaderFile, TypedArray<ComputeShaderKernel>, kernels)
