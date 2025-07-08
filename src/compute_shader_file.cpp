#include "compute_shader_file.h"

void ComputeShaderFile::_bind_methods(){
	BIND_GET_SET_RESOURCE_ARRAY(ComputeShaderFile, kernels, ComputeShaderKernel)
	BIND_GET_SET(ComputeShaderFile, base_error, Variant::STRING)
}

GET_SET_PROPERTY_IMPL(ComputeShaderFile, TypedArray<ComputeShaderKernel>, kernels)
GET_SET_PROPERTY_IMPL(ComputeShaderFile, String, base_error)
