#include "slang_module.h"

#include "reflection_context.h"

using namespace gdslang;
using namespace godot;

void SlangModule::_bind_methods() {
	BIND_GET_SET(SlangModule, diagnostic, Variant::STRING);
	BIND_METHOD(SlangModule, get_params_shape)
	BIND_METHOD(SlangModule, to_json)
}

slang::IModule* SlangModule::get_module() const {
	return module.get();
}

void SlangModule::set_module(slang::IModule* p_module) {
	module = p_module;
}

slang::ProgramLayout* SlangModule::get_layout() const {
	ERR_FAIL_NULL_V(module, nullptr);
	return module->getLayout();
}

String SlangModule::get_file_path() const {
	ERR_FAIL_NULL_V(module, {});
	return module->getFilePath();
}

Ref<StructTypeLayoutShape> SlangModule::get_params_shape() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.get_params_shape();
}

Variant SlangModule::to_json() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.to_json();
}

GET_SET_PROPERTY_IMPL(SlangModule, String, diagnostic)
