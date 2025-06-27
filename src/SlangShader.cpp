#include "SlangShader.h"

void SlangShader::_bind_methods() {
	BIND_GET_SET_RESOURCE(SlangShader, shader_file, RDShaderFile)
}

GET_SET_PROPERTY_IMPL(SlangShader, Ref<RDShaderFile>, shader_file)
