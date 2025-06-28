#include "slang_shader.h"

void SlangShader::_bind_methods() {
	BIND_GET_SET_RESOURCE_ARRAY(SlangShader, kernels, RDShaderFile)
}

GET_SET_PROPERTY_IMPL(SlangShader, TypedArray<ComputeShaderKernel>, kernels)
