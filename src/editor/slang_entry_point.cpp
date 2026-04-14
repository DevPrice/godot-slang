#include "binding_macros.h"
#include "reflection_context.h"

#include "slang_entry_point.h"

using namespace godot;

void SlangEntryPoint::_bind_methods(){
	BIND_METHOD(SlangEntryPoint, get_name)
}

slang::IEntryPoint* SlangEntryPoint::get_entry_point() const {
	return entry_point.get();
}

slang::IEntryPoint** SlangEntryPoint::write_ref() {
	return entry_point.writeRef();
}

String SlangEntryPoint::get_name() const {
	ERR_FAIL_NULL_V(entry_point, {});
	slang::FunctionReflection* reflection = entry_point->getFunctionReflection();
	ERR_FAIL_NULL_V(reflection, {});
	return reflection->getName();
}
