#include "binding_macros.h"
#include "reflection_context.h"

#include "slang_entry_point.h"

using namespace godot;

void SlangEntryPoint::_bind_methods() {
	BIND_METHOD(SlangEntryPoint, get_name)
}

slang::IEntryPoint* SlangEntryPoint::get_entry_point() const {
	return entry_point.get();
}

slang::IEntryPoint** SlangEntryPoint::write_ref() {
	return entry_point.writeRef();
}

slang::IComponentType* SlangEntryPoint::get_component_type() const {
	return get_entry_point();
}

Ref<StructTypeLayoutShape> SlangEntryPoint::get_params_shape() const {
	slang::ProgramLayout* layout = get_layout();
	ERR_FAIL_NULL_V(layout, nullptr);
	const SlangReflectionContext reflection_context(layout);
	// TODO: Will this always be index zero?
	return reflection_context.get_entry_point_params_shape(layout->getEntryPointByIndex(0));
}

String SlangEntryPoint::get_name() const {
	ERR_FAIL_NULL_V(entry_point, {});
	slang::FunctionReflection* reflection = entry_point->getFunctionReflection();
	ERR_FAIL_NULL_V(reflection, {});
	return reflection->getName();
}
