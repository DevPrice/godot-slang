#include "slang_module.h"

using namespace gdslang;
using namespace godot;

void SlangModule::_bind_methods() {
	BIND_GET_SET(SlangModule, diagnostic, Variant::STRING);
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

GET_SET_PROPERTY_IMPL(SlangModule, String, diagnostic)
