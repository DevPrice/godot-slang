#include "reflection_context.h"

#include "slang_component_type.h"

using namespace godot;

void SlangComponentType::_bind_methods() {
	BIND_GET_SET(SlangComponentType, diagnostic, Variant::STRING)
	BIND_METHOD(SlangComponentType, link)
	ClassDB::bind_method(D_METHOD("compile_entry_point","entry_point_index", "target_index"), &SlangComponentType::compile_entry_point, DEFVAL(0), DEFVAL(0));
}

slang::IComponentType* SlangComponentType::get_component_type() const {
	return component_type.get();
}

slang::ProgramLayout* SlangComponentType::get_layout() const {
	slang::IComponentType* component_type_ptr = get_component_type();
	ERR_FAIL_NULL_V(component_type_ptr, nullptr);
	return component_type_ptr->getLayout();
}

Ref<StructTypeLayoutShape> SlangComponentType::get_params_shape() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.get_params_shape();
}

Variant SlangComponentType::to_json() const {
	const SlangReflectionContext reflection_context(get_layout());
	return reflection_context.to_json();
}

Ref<SlangComponentType> SlangComponentType::link() const {
	ERR_FAIL_NULL_V(component_type, nullptr);
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	slang::IComponentType* linked_component;
	ERR_FAIL_COND_V(SLANG_FAILED(component_type->link(&linked_component, diagnostics_blob.writeRef())), nullptr);
	if (diagnostics_blob) {
		return create(linked_component, SlangBlob::blob_to_string(diagnostics_blob));
	}
	return create(linked_component);
}

Ref<SlangBlob> SlangComponentType::compile_entry_point(const int64_t entry_point_index, const int64_t target_index) const {
	ERR_FAIL_NULL_V(component_type, {});
	Slang::ComPtr<slang::IBlob> entry_point_blob;
	Slang::ComPtr<slang::IBlob> diagnostics_blob;
	ERR_FAIL_COND_V(SLANG_FAILED(component_type->getEntryPointCode(entry_point_index, target_index, entry_point_blob.writeRef(), diagnostics_blob.writeRef())), nullptr);
	return SlangBlob::create(entry_point_blob.get(), diagnostics_blob.get());
}

Ref<SlangComponentType> SlangComponentType::create(slang::IComponentType* component_type, const String& diagnostic) {
	Ref ret = memnew(SlangComponentType);
	ret->component_type = component_type;
	ret->set_diagnostic(diagnostic);
	return ret;
}

GET_SET_PROPERTY_IMPL(SlangComponentType, String, diagnostic)
