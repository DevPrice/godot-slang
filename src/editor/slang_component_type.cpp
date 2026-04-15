#include "slang_component_type.h"

using namespace godot;

void SlangComponentType::_bind_methods() {
	BIND_GET_SET(SlangComponentType, diagnostic, Variant::STRING);
}

slang::IComponentType* SlangComponentType::get_component_type() const {
	return component_type.get();
}

Ref<SlangComponentType> SlangComponentType::create(slang::IComponentType* component_type, const String& diagnostic) {
	Ref ret = memnew(SlangComponentType);
	ret->component_type = component_type;
	ret->set_diagnostic(diagnostic);
	return ret;
}

GET_SET_PROPERTY_IMPL(SlangComponentType, String, diagnostic)
