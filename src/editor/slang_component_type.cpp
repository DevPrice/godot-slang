#include "slang_component_type.h"

using namespace godot;

void SlangComponentType::_bind_methods() {
	BIND_GET_SET(SlangComponentType, diagnostic, Variant::STRING)
	BIND_METHOD(SlangComponentType, link)
}

slang::IComponentType* SlangComponentType::get_component_type() const {
	return component_type.get();
}

Ref<SlangComponentType> SlangComponentType::link() const {
	ERR_FAIL_NULL_V(component_type, {});
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	slang::IComponentType* linked_component;
	component_type->link(&linked_component, diagnostics_blob.writeRef());
	if (diagnostics_blob) {
		return create(linked_component, String::utf8(static_cast<const char*>(diagnostics_blob->getBufferPointer()), diagnostics_blob->getBufferSize()));
	}
	return create(linked_component);
}

Ref<SlangComponentType> SlangComponentType::create(slang::IComponentType* component_type, const String& diagnostic) {
	Ref ret = memnew(SlangComponentType);
	ret->component_type = component_type;
	ret->set_diagnostic(diagnostic);
	return ret;
}

GET_SET_PROPERTY_IMPL(SlangComponentType, String, diagnostic)
